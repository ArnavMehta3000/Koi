#include "Koi/Engine/Graphics/ShaderCache.h"
#include "Koi/Engine/Graphics/ShaderCompiler.h"
#include "Koi/Engine/IO/VirtualFileSystem.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Text/StringUtils.h"
#include <algorithm>

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

	ShaderType InferShaderTypeFromName(std::string_view path)
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

	u64 HashShaderDefines(std::span<const ShaderDefine> defines)
	{
		u64 hash = kFnvOffset;
		for (const ShaderDefine& define : defines)
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
		const ShaderType type = InferShaderTypeFromName(virtualPathNoExt);
		if (type == ShaderType::Unknown)
		{
			KOI_LOG_ERROR("ShaderCache: cannot infer type from '{}' (expected *VS/*PS/*CS/*HS/*DS/*GS)", virtualPathNoExt);
			return {};
		}

		const ShaderKey key
		{
			.Path        = std::string(virtualPathNoExt),
			.Type        = type,
			.DefinesHash = HashShaderDefines(defines),
		};

		if (auto it = m_entries.find(key); it != m_entries.end())
		{
			KOI_LOG_TRACE("Shader cache hit: '{}'", virtualPathNoExt);
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
			KOI_LOG_INFO("Shader cache: nothing changed on disk");

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

	// Editor: compile "<path>.hlsl" from the source tree | Runtime: read the cooked "<path>.cso"
	// Defines only affect the compile path: Cook writes one .cso per source
	bool ShaderCache::LoadBytecode(const ShaderKey& key, std::span<const ShaderDefine> defines, Entry& outEntry) const
	{
		const std::string hlslPath = key.Path + ".hlsl";
		const auto resolved = VirtualFileSystem::Get().TryResolveToDisk(hlslPath);
		if (!resolved || !fs::exists(*resolved))
		{
			return LoadCooked(key, outEntry);
		}

		// DxcDefine only holds pointers, so the wide strings have to outlive the compile call
		std::vector<std::wstring> wide;
		wide.reserve(defines.size() * 2);
		for (const ShaderDefine& define : defines)
		{
			wide.push_back(StringUtils::ToWide(define.Name));
			wide.push_back(StringUtils::ToWide(define.Value));
		}

		// OutDir intentionally left empty - the editor compiles in-memory only
		ShaderCompiler::Args args{ .Type = key.Type };
		args.Defines.reserve(defines.size());
		for (size_t i = 0; i < defines.size(); ++i)
		{
			args.Defines.push_back(DxcDefine{ wide[i * 2].c_str(), wide[i * 2 + 1].c_str() });
		}

		std::vector<fs::path> includes;
		auto blob = ShaderCompiler::Get().CompileFromFile(*resolved, args, &includes);
		if (!blob || blob->GetBufferSize() == 0)
		{
			KOI_LOG_ERROR("ShaderCache: compile failed for '{}'", hlslPath);
			return false;
		}

		const auto* first = static_cast<const byte*>(blob->GetBufferPointer());
		outEntry.Bytecode.assign(first, first + blob->GetBufferSize());

		outEntry.Dependencies.reserve(includes.size() + 1);
		AddDependency(outEntry, *resolved);
		for (const fs::path& include : includes)
		{
			AddDependency(outEntry, include);
		}

		return true;
	}

	// Dependencies are left empty - nothing recompiles from a .cso, so nothing goes stale
	bool ShaderCache::LoadCooked(const ShaderKey& key, Entry& outEntry) const
	{
		const std::string csoPath = key.Path + ".cso";
		auto data = VirtualFileSystem::Get().Read(csoPath);
		if (!data)
		{
			KOI_LOG_ERROR("ShaderCache: neither '{}.hlsl' nor '{}.cso' could be loaded", key.Path, key.Path);
			return false;
		}

		KOI_LOG_TRACE("Loaded shader bytecode '{}' ({} bytes)", csoPath, data->size());
		outEntry.Bytecode = std::move(*data);
		return true;
	}
}
