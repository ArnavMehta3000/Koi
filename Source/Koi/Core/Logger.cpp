#include "Core/Logger.h"
#include <filesystem>

namespace koi
{
    namespace fs = std::filesystem;

	std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> Logger::s_loggers;
	std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> Logger::s_consoleSink;
	std::shared_ptr<spdlog::sinks::basic_file_sink_mt> Logger::s_fileSink;
	bool Logger::s_initialized = false;

    void Logger::Initialize(const std::string& logDir)
    {
        if (s_initialized) 
            return;

        fs::create_directories(logDir);

        try
        {
            s_consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            s_consoleSink->set_level(spdlog::level::trace);

            std::string logFile = logDir + "/koi.log";
            s_fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFile, true);
            s_fileSink->set_level(spdlog::level::trace);

            std::string pattern = "[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v";
            s_consoleSink->set_pattern(pattern);
            s_fileSink->set_pattern(pattern);

            GetOrCreateLogger(log::Core);

            s_initialized = true;
        }
        catch (const spdlog::spdlog_ex& ex)
        {
            auto errMsg = fmt::format("Failed to initialize logger: {}", ex.what());
            KOI_ASSERT_MSG(false, errMsg);
        }
    }

    void Logger::Shutdown()
    {
        if (!s_initialized)
            return;

        Flush();
        spdlog::drop_all();
        s_loggers.clear();
        s_initialized = false;
    }

    std::shared_ptr<spdlog::logger> Logger::GetLogger(const std::string& category)
    {
        auto it = s_loggers.find(category);
        if (it != s_loggers.end())
        {
            return it->second;
        }
        return nullptr;
    }

    std::shared_ptr<spdlog::logger> Logger::GetOrCreateLogger(const std::string& category)
    {
        std::shared_ptr<spdlog::logger> logger = GetLogger(category);
        if (logger.get())
        {
            return logger;
        }

        return CreateLogger(category);
    }

    void Logger::SetLevel(Level level)
    {
        auto spdlogLevel = ToSpdlogLevel(level);
    
        if (s_consoleSink) s_consoleSink->set_level(spdlogLevel);
        if (s_fileSink) s_fileSink->set_level(spdlogLevel);

        for (auto& [name, logger] : s_loggers)
        {
            logger->set_level(spdlogLevel);
        }
    }

    void Logger::SetCategoryLevel(const std::string& category, Level level)
    {
        if (auto logger = GetOrCreateLogger(category))
        {
            logger->set_level(ToSpdlogLevel(level));
        }
    }

    void Logger::SetPattern(const std::string& pattern)
    {
        if (s_consoleSink) s_consoleSink->set_pattern(pattern);
        if (s_fileSink) s_fileSink->set_pattern(pattern);
    }

    void Logger::Flush()
    {
        spdlog::drop_all();
        spdlog::flush_on(spdlog::level::err);
    }

    std::shared_ptr<spdlog::logger> Logger::CreateLogger(const std::string& category)
    {
        if (!s_initialized)
            return nullptr;

        try
        {
            std::vector<spdlog::sink_ptr> sinks;
            sinks.push_back(s_consoleSink);
            sinks.push_back(s_fileSink);

            auto logger = std::make_shared<spdlog::logger>(category, sinks.begin(), sinks.end());
            logger->set_level(spdlog::level::trace);
            logger->flush_on(spdlog::level::err);

            s_loggers[category] = logger;
            spdlog::register_logger(logger);

            return logger;
        }
        catch (const spdlog::spdlog_ex& ex)
        {
            auto errMsg = fmt::format("Failed to create logger: {}", ex.what());
			KOI_ASSERT_MSG(false, errMsg);

            return nullptr;
        }
    }

    spdlog::level::level_enum Logger::ToSpdlogLevel(Level level)
    {
        switch (level)
        {
        case Level::Trace:    return spdlog::level::trace;
        case Level::Debug:    return spdlog::level::debug;
        case Level::Info:     return spdlog::level::info;
        case Level::Warn:     return spdlog::level::warn;
        case Level::Error:    return spdlog::level::err;
        case Level::Critical: return spdlog::level::critical;
        default:              return spdlog::level::info;
        }
    }
}