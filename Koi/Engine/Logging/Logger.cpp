#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Logging/LogHistory.h"
#include "Koi/Engine/Core/Win32.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <deque>
#include <filesystem>
#include <iostream>
#include <map>
#include <mutex>
#include <ranges>
#include <unordered_map>

namespace fs = std::filesystem;

namespace koi
{
	spdlog::logger* Logger::s_logger = nullptr;
	constinit std::array<std::atomic<u8>, Logger::kMaxCategories> Logger::s_levels{};

	bool Logger::ShouldLog(u16 category, LogLevel level)
	{
		return s_logger && static_cast<u8>(level) >= s_levels[category].load(std::memory_order_relaxed);
	}

	static_assert(static_cast<int>(LogLevel::Trace)    == spdlog::level::trace);
	static_assert(static_cast<int>(LogLevel::Debug)    == spdlog::level::debug);
	static_assert(static_cast<int>(LogLevel::Info)     == spdlog::level::info);
	static_assert(static_cast<int>(LogLevel::Warn)     == spdlog::level::warn);
	static_assert(static_cast<int>(LogLevel::Error)    == spdlog::level::err);
	static_assert(static_cast<int>(LogLevel::Critical) == spdlog::level::critical);

	namespace
	{
		struct SourceEntry
		{
			std::string Path;
			u32         NameOffset = 0;  // start of the basename within Path
		};

		struct SourceTable
		{
			std::mutex                           Mutex;
			std::unordered_map<std::string, u16> Ids;
			std::deque<SourceEntry>              Entries;  // deque: references must stay stable
			std::atomic<u32>                     Count{ 0 };
		};

		struct CategoryEntry
		{
			std::string Name;
			LogOrigin   Origin = LogOrigin::Engine;
		};

		struct CategoryTable
		{
			std::mutex                                   Mutex;
			std::deque<CategoryEntry>                    Entries;
			std::atomic<u32>                             Count{ 0 };
			LogLevel                                     DefaultLevel = LogLevel::Debug;
			std::map<std::string, LogLevel, std::less<>> Overrides;

			[[nodiscard]] LogLevel LevelFor(std::string_view name) const
			{
				const auto it = Overrides.find(name);
				return it != Overrides.end() ? it->second : DefaultLevel;
			}
		};

		SourceTable& Sources()
		{
			static SourceTable table;
			return table;
		}

		CategoryTable& Categories()
		{
			static CategoryTable table;
			return table;
		}

		std::shared_ptr<spdlog::logger>            g_logger;
		std::chrono::steady_clock::time_point      g_start;
		bool                                       g_fileTruncated = false;

		f64 ElapsedSeconds()
		{
			using namespace std::chrono;
			return duration<f64>(steady_clock::now() - g_start).count();
		}

		bool HasConsoleAttached()
		{
			return ::GetConsoleWindow() != nullptr;
		}

		void SpawnConsoleWindow()
		{
			if (HasConsoleAttached()) return;
			if (!::AllocConsole()) return;

			FILE* f = nullptr;
			::freopen_s(&f, "CONOUT$", "w", stdout);
			::freopen_s(&f, "CONOUT$", "w", stderr);
			::freopen_s(&f, "CONIN$", "r", stdin);
			std::ios::sync_with_stdio(true);
		}
	}

	LogCategory::LogCategory(std::string_view name, LogOrigin origin)
		: Id(Logger::RegisterCategory(name, origin))
	{
	}

	void Logger::Init(const LoggerConfig& cfg)
	{
		if (s_logger) return;
		g_start = std::chrono::steady_clock::now();
		Configure(cfg);
	}

	void Logger::Configure(const LoggerConfig& cfg)
	{
		// %! is the category, %s the source basename and %# the line - all from the source_loc we pass
		constexpr auto pattern = "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%!] [%s:%#] %v";

		std::vector<spdlog::sink_ptr> sinks;

		try
		{
			if (!cfg.FilePath.empty())
			{
				const fs::path path = cfg.FilePath;
				if (path.has_parent_path())
				{
					fs::create_directories(path.parent_path());
				}

				// Only truncate the first time, so a reconfigure doesn't discard startup logs
				sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(path.string(), !g_fileTruncated));
				g_fileTruncated = true;
			}

			if (cfg.UseDebugSink)
				sinks.push_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());

			if (cfg.SpawnConsole)
				SpawnConsoleWindow();

			// Console sink only if a console is actually attached to the process
			if (HasConsoleAttached())
				sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());

			for (spdlog::sink_ptr& sink : sinks)
			{
				sink->set_level(spdlog::level::trace);
				sink->set_pattern(pattern);
			}

			// Category levels do the filtering, the logger itself passes everything
			auto logger = std::make_shared<spdlog::logger>("koi", sinks.begin(), sinks.end());
			logger->set_level(spdlog::level::trace);
			logger->flush_on(spdlog::level::err);

			// Deliberately not registered with spdlog's global registry, that is what made spdlog::shutdown() crash the program at exit
			g_logger = std::move(logger);
			s_logger = g_logger.get();
		}
		catch (const spdlog::spdlog_ex& ex)
		{
			// Missing log file/sink, carry on without that sink
			std::cerr << "Logger configuration failed: " << ex.what() << std::endl;
		}

		LogHistory::Get().Resize(cfg.HistoryCapacity);

		std::vector<std::string> unknown;
		{
			CategoryTable& table = Categories();
			std::lock_guard lock(table.Mutex);

			table.DefaultLevel = cfg.RuntimeLevel;
			table.Overrides    = cfg.CategoryLevels;
			for (size_t i = 0; i < table.Entries.size(); ++i)
			{
				s_levels[i].store(static_cast<u8>(table.LevelFor(table.Entries[i].Name)), std::memory_order_relaxed);
			}

			for (const auto& name : table.Overrides | std::views::keys)
			{
				if (std::ranges::none_of(table.Entries, [&](const CategoryEntry& e) { return e.Name == name; }))
					unknown.push_back(name);
			}
		}

		for (const std::string& name : unknown)
		{
			KOI_LOG(Engine, Warn, "Log level given for '{}', but no KOI_LOG_CATEGORY declares it", name);
		}
	}

	void Logger::Shutdown()
	{
		if (!s_logger) return;


		Flush();
		s_logger = nullptr;
		g_logger.reset();
	}

	void Logger::Flush()
	{
		if (s_logger)
			s_logger->flush();
	}

	u16 Logger::RegisterCategory(std::string_view name, LogOrigin origin)
	{
		CategoryTable& table = Categories();
		std::lock_guard lock(table.Mutex);

		for (size_t i = 0; i < table.Entries.size(); ++i)
		{
			if (table.Entries[i].Name == name)
				return static_cast<u16>(i);
		}

		if (table.Entries.size() >= kMaxCategories)
			return 0;

		const u16 id = static_cast<u16>(table.Entries.size());
		table.Entries.push_back(CategoryEntry{ .Name = std::string(name), .Origin = origin });
		s_levels[id].store(static_cast<u8>(table.LevelFor(name)), std::memory_order_relaxed);
		table.Count.store(id + 1u, std::memory_order_release);
		return id;
	}

	LogCategoryInfo Logger::GetCategory(u16 id)
	{
		CategoryTable& table = Categories();

		// Entries only ever grow and a deque never moves them, so anything below the
		// published count is safe to read without the lock
		if (id >= table.Count.load(std::memory_order_acquire))
			return {};

		const CategoryEntry& entry = table.Entries[id];
		return { entry.Name, entry.Origin };
	}

	u16 Logger::CategoryCount()
	{
		return static_cast<u16>(Categories().Count.load(std::memory_order_acquire));
	}

	u16 Logger::InternSource(std::string_view path)
	{
		SourceTable& table = Sources();
		std::string key(path);

		std::lock_guard lock(table.Mutex);

		if (auto it = table.Ids.find(key); it != table.Ids.end())
			return it->second;

		if (table.Entries.size() >= 0xFFFF)
			return 0;  // absurd; keep ids in range rather than wrap

		const size_t slash  = path.find_last_of("/\\");
		const u32    offset = (slash == std::string_view::npos) ? 0u : static_cast<u32>(slash + 1);
		const u16    id     = static_cast<u16>(table.Entries.size());

		table.Entries.push_back(SourceEntry{ .Path = key, .NameOffset = offset });
		table.Ids.emplace(std::move(key), id);
		table.Count.store(id + 1u, std::memory_order_release);
		return id;
	}

	LogSourceInfo Logger::GetSource(u16 id)
	{
		SourceTable& table = Sources();
		if (id >= table.Count.load(std::memory_order_acquire))
			return {};

		const SourceEntry& entry = table.Entries[id];
		return { entry.Path, std::string_view(entry.Path).substr(entry.NameOffset) };
	}

	void Logger::Log(u16 category, LogLevel level, u16 source, u32 line, std::string_view message)
	{
		if (!s_logger) return;
		KOI_ZONE_UNMUTED;

		const LogSourceInfo   file = GetSource(source);
		const LogCategoryInfo cat  = GetCategory(category);

		// Both views point into deque-held strings, so they are null-terminated
		s_logger->log(spdlog::source_loc{ file.Path.empty() ? "" : file.Path.data(), static_cast<int>(line), cat.Name.empty() ? "" : cat.Name.data() },
		              static_cast<spdlog::level::level_enum>(level),
		              spdlog::string_view_t(message.data(), message.size()));

		LogHistory::Get().Push(level, category, source, line, ElapsedSeconds(), message);
	}
}
