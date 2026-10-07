#include "Koi/Engine/Graphics/ShaderCache.h"
#include "Koi/Engine/Graphics/ShaderCompiler.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/IO/VirtualFileSystem.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Text/StringUtils.h"
#include <algorithm>
#include <toml++/toml.hpp>

namespace koi
{
	namespace
	{
		// Stuff for Fowler–Noll–Vo hashing
		constexpr u64 kFnvOffset = 14695981039346656037ull;
		constexpr u64 kFnvPrime  = 1099511628211ull;

		static void FnvFeed(u64& hash, std::string_view text)
		{
			for (const char c : text)
			{
				hash ^= static_cast<u64>(static_cast<byte>(c));
				hash *= kFnvPrime;
			}
		}

		static void HashCombine(size_t& seed, size_t value)
		{
			seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
		}
	}

	size_t ShaderKey::Hash::operator()(const ShaderKey& key) const noexcept
	{
		size_t seed = std::hash<std::string>{}(key.Path);
		HashCombine(seed, static_cast<size_t>(key.Type));
		HashCombine(seed, static_cast<size_t>(key.DefinesHash));
		return seed;
	}

	ShaderType ShaderCache::InferType(std::string_view path)
	{
		// Strip directory + (optional) extension to get the stem.
		auto slash = path.find_last_of("/\\");
		std::string_view stem = slash == std::string_view::npos ? path : path.substr(slash + 1);
		auto dot = stem.find_last_of('.');
		if (dot != std::string_view::npos) stem = stem.substr(0, dot);

		if (stem.size() < 2) return ShaderType::Unknown;
		std::string_view suffix = stem.substr(stem.size() - 2);
		if (suffix == "VS") return ShaderType::Vertex;
		if (suffix == "PS") return ShaderType::Pixel;
		if (suffix == "CS") return ShaderType::Compute;
		if (suffix == "HS") return ShaderType::Hull;
		if (suffix == "DS") return ShaderType::Domain;
		if (suffix == "GS") return ShaderType::Geometry;
		return ShaderType::Unknown;
	}

	u64 ShaderCache::HashDefines(std::span<const ShaderDefine> defines)
	{
		std::vector<ShaderDefine> sorted(defines.begin(), defines.end());
		std::ranges::sort(sorted, {}, &ShaderDefine::Name);

		u64 hash = kFnvOffset;
		for (const ShaderDefine& define : sorted)
		{
			FnvFeed(hash, define.Name);
			FnvFeed(hash, "=");
			FnvFeed(hash, define.Value);
			FnvFeed(hash, ";");
		}
		return hash;
	}

	ShaderCache& ShaderCache::Get()
	{
		static ShaderCache instance;
		return instance;
	}

	std::span<const byte> ShaderCache::GetShader(std::string_view virtualPathNoExt, std::span<const ShaderDefine> defines)
	{
		LoadManifests();

		const ShaderType type = InferType(virtualPathNoExt);
		if (type == ShaderType::Unknown)
		{
			KOI_LOG_ERROR("ShaderCache: cannot infer type from '{}' (expected *VS/*PS/*CS/*HS/*DS/*GS)", virtualPathNoExt);
			return {};
		}

		const ShaderKey key
		{
			.Path        = std::string(virtualPathNoExt),
			.Type        = type,
			.DefinesHash = HashDefines(defines),
		};

		if (auto it = m_entries.find(key); it != m_entries.end())
		{
			return it->second.Bytecode;
		}

		Entry entry;
		if (!LoadBytecode(key, defines, entry))
			return {};  // failures aren't cached, so a fixed shader loads on the next attempt

		KOI_LOG_DEBUG("Shader cached: '{}' ({} bytes, {} dependencies)",
			virtualPathNoExt, entry.Bytecode.size(), entry.Dependencies.size());

		return m_entries.emplace(key, std::move(entry)).first->second.Bytecode;
	}

	std::vector<std::string> ShaderCache::InvalidateStale()
	{
		// xmake may have rebuilt while we were running, so read the manifests again next time
		m_built.clear();
		m_manifestsLoaded = false;

		std::vector<std::string> dropped;
		for (auto it = m_entries.begin(); it != m_entries.end(); )
		{
			const fs::path* changed = FindChangedDependency(it->second);
			if (!changed)
			{
				++it;
				continue;
			}

			KOI_LOG_INFO("Shader stale: '{}' (changed: {})", it->first.Path, changed->filename().string());

			// One path can have several variants cached, and the caller only wants the path
			if (std::ranges::find(dropped, it->first.Path) == dropped.end())
				dropped.push_back(it->first.Path);

			it = m_entries.erase(it);
		}

		if (dropped.empty())
		{
			KOI_LOG_INFO("Shader cache: nothing changed on disk");
		}

		return dropped;
	}

	bool ShaderCache::IsCompiled(std::string_view virtualPathNoExt) const
	{
		return std::ranges::any_of(m_entries, [&](const auto& entry)
		{
			return entry.first.Path == virtualPathNoExt;
		});
	}

	void ShaderCache::InvalidateAll()
	{
		KOI_LOG_INFO("Shader cache invalidated ({} entries dropped)", m_entries.size());
		m_entries.clear();
		m_built.clear();
		m_manifestsLoaded = false;
	}

	ShaderBuildState ShaderCache::GetBuildState(std::string_view virtualPathNoExt)
	{
		LoadManifests();

		ShaderBuildState state = ShaderBuildState::Missing;
		for (const auto& [key, built] : m_built)
		{
			if (key.Path != virtualPathNoExt) continue;
			if (IsStale(built)) return ShaderBuildState::Stale;
			state = ShaderBuildState::UpToDate;
		}
		return state;
	}

	void ShaderCache::LoadManifests()
	{
		if (m_manifestsLoaded) return;
		m_manifestsLoaded = true;

		ZoneScoped;

		for (const std::string_view scheme : { "engine", "project" })
		{
			const std::string manifestPath = std::string(scheme) + "://Shaders/Manifest.toml";
			const std::optional<std::vector<byte>> bytes = VirtualFileSystem::Get().Read(manifestPath);
			if (!bytes)
			{
				// Projects without a Shaders.json simply have nothing here
				if (scheme == "engine")
				{
					KOI_LOG_WARN("No prebuilt engine shaders ({} is missing). Build with xmake, until then they compile on first use", manifestPath);
				}
				continue;
			}

			toml::table root;
			try
			{
				root = toml::parse(std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()), manifestPath);
			}
			catch (const toml::parse_error& e)
			{
				KOI_LOG_ERROR("Couldn't make sense of {}: {}", manifestPath, e.description());
				continue;
			}

			const toml::array* shaders = root["shader"].as_array();
			if (!shaders) continue;

			size_t count = 0;
			for (const toml::node& node : *shaders)
			{
				const toml::table* shader = node.as_table();
				if (!shader) continue;

				const std::string source = (*shader)["source"].value_or(std::string());
				const std::string cso    = (*shader)["cso"].value_or(std::string());
				if (source.empty() || cso.empty()) continue;

				std::vector<ShaderDefine> defines;
				if (const toml::array* list = (*shader)["defines"].as_array())
				{
					for (const toml::node& define : *list)
					{
						const std::string_view text = define.value_or(std::string_view());
						const size_t equals = text.find('=');
						defines.push_back(ShaderDefine
						{
							.Name  = std::string(text.substr(0, equals)),
							.Value = equals == std::string_view::npos ? std::string() : std::string(text.substr(equals + 1)),
						});
					}
				}

				BuiltShader built{ .Bytecode = std::string(scheme) + "://Shaders/" + cso };
				if (const toml::array* list = (*shader)["dependencies"].as_array())
				{
					for (const toml::node& dependency : *list)
						built.Dependencies.emplace_back(dependency.value_or(std::string_view()));
				}

				const ShaderKey key
				{
					.Path        = source,
					.Type        = InferType(source),
					.DefinesHash = HashDefines(defines),
				};
				m_built.insert_or_assign(key, std::move(built));
				++count;
			}

			KOI_LOG_DEBUG("Found {} prebuilt shader(s) for {}://", count, scheme);
		}
	}

	const fs::path* ShaderCache::FindChangedDependency(const Entry& entry)
	{
		for (const Dependency& dependency : entry.Dependencies)
		{
			std::error_code ec;
			const auto stamp = fs::last_write_time(dependency.Path, ec);

			// A file we can't stat is deleted or mid-write, so treat it as changed
			if (ec || stamp != dependency.Stamp)
				return &dependency.Path;
		}
		return nullptr;
	}

	void ShaderCache::AddDependency(Entry& entry, const fs::path& path)
	{
		std::error_code ec;
		entry.Dependencies.push_back(Dependency{ .Path = path, .Stamp = fs::last_write_time(path, ec) });
	}

	bool ShaderCache::LoadBytecode(const ShaderKey& key, std::span<const ShaderDefine> defines, Entry& outEntry) const
	{
		// Only the editor has sources on disk, which is what lets it recompile
		const auto source    = VirtualFileSystem::Get().TryResolveToDisk(key.Path + ".hlsl");
		const bool hasSource = source && fs::exists(*source);

		if (const auto it = m_built.find(key); it != m_built.end())
		{
			if (!hasSource || !IsStale(it->second))
				return LoadBuilt(it->second, hasSource, outEntry);

			KOI_LOG_INFO("{} changed since the last build, compiling it fresh", key.Path);
		}
		else if (hasSource)
		{
			KOI_LOG_INFO("{} isn't built by any Shaders.json, compiling it now. List it there so the build catches its errors", key.Path);
		}

		if (!hasSource)
		{
			KOI_LOG_ERROR("{} wasn't built and there's no source to compile it from. Is it missing from Shaders.json?", key.Path);
			return false;
		}

		return Compile(key, defines, *source, outEntry);
	}

	bool ShaderCache::Compile(const ShaderKey& key, std::span<const ShaderDefine> defines, const fs::path& source, Entry& outEntry) const
	{
		ZoneScoped;

		// DxcDefine only holds pointers, so the wide strings have to outlive the compile call
		std::vector<std::wstring> wide;
		wide.reserve(defines.size() * 2);
		for (const ShaderDefine& define : defines)
		{
			wide.push_back(utils::ToWide(define.Name));
			wide.push_back(utils::ToWide(define.Value));
		}

		// OutDir intentionally left empty - the editor compiles in-memory only
		ShaderCompiler::Args args{ .Type = key.Type };
		args.Defines.reserve(defines.size());
		for (size_t i = 0; i < defines.size(); ++i)
		{
			args.Defines.push_back(DxcDefine{ wide[i * 2].c_str(), wide[i * 2 + 1].c_str() });
		}

		std::vector<fs::path> includes;
		auto blob = ShaderCompiler::Get().CompileFromFile(source, args, &includes);
		if (!blob || blob->GetBufferSize() == 0)
		{
			KOI_LOG_ERROR("{} didn't compile, the errors are just above", key.Path);
			return false;
		}

		const auto* first = static_cast<const byte*>(blob->GetBufferPointer());
		outEntry.Bytecode.assign(first, first + blob->GetBufferSize());

		outEntry.Dependencies.reserve(includes.size() + 1);
		AddDependency(outEntry, source);
		for (const fs::path& include : includes)
		{
			AddDependency(outEntry, include);
		}

		return true;
	}

	bool ShaderCache::LoadBuilt(const BuiltShader& built, bool trackDependencies, Entry& outEntry) const
	{
		ZoneScoped;

		std::optional<std::vector<byte>> data = VirtualFileSystem::Get().Read(built.Bytecode);
		if (!data)
		{
			KOI_LOG_ERROR("Couldn't read {}. The shader build looks incomplete, try rebuilding with xmake", built.Bytecode);
			return false;
		}

		outEntry.Bytecode = std::move(*data);

		// So F5 notices edits made from here on
		if (trackDependencies)
		{
			for (const std::string& dependency : built.Dependencies)
			{
				if (const auto path = VirtualFileSystem::Get().TryResolveToDisk(dependency))
					AddDependency(outEntry, *path);
			}
		}

		KOI_LOG_TRACE("Loaded prebuilt shader {} ({} bytes)", built.Bytecode, outEntry.Bytecode.size());
		return true;
	}

	bool ShaderCache::IsStale(const BuiltShader& built)
	{
		VirtualFileSystem& vfs = VirtualFileSystem::Get();

		std::error_code ec;
		const auto bytecode = vfs.TryResolveToDisk(built.Bytecode);
		if (!bytecode) return true;

		const auto builtAt = fs::last_write_time(*bytecode, ec);
		if (ec) return true;

		for (const std::string& dependency : built.Dependencies)
		{
			const auto path = vfs.TryResolveToDisk(dependency);
			if (!path) return true;

			const auto changedAt = fs::last_write_time(*path, ec);
			if (ec || changedAt > builtAt) return true;
		}
		return false;
	}
}
