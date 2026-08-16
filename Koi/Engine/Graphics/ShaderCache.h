#pragma once
#include "Koi/Engine/Graphics/Types.h"
#include <filesystem>
#include <span>
#include <unordered_map>

namespace koi
{
	namespace fs = std::filesystem;

	struct ShaderDefine
	{
		std::string Name;
		std::string Value;
	};

	// Identifies one compiled variant of one shader source
	struct ShaderKey
	{
		std::string Path;       // virtual path, no extension
		ShaderType  Type        = ShaderType::Unknown;
		u64         DefinesHash = 0;

		bool operator==(const ShaderKey&) const = default;

		struct Hash
		{
			[[nodiscard]] size_t operator()(const ShaderKey& key) const noexcept;
		};
	};

	// *VS -> Vertex, *PS -> Pixel, etc
	[[nodiscard]] ShaderType InferShaderTypeFromName(std::string_view path);

	// Order sensitive, so callers must list their defines consistently
	[[nodiscard]] u64 HashShaderDefines(std::span<const ShaderDefine> defines);

	// Owns the bytecode of every shader variant loaded, keyed by source path, stage and defines
	// Editor builds compile "<path>.hlsl" on demand, runtime builds read the cooked "<path>.cso"
	class ShaderCache : public NonCopyable
	{
	public:
		static ShaderCache& Get();

		// Bytecode for a virtual path WITHOUT extension ("engine://Assets/Shaders/MeshVS").
		// The span points into cache storage and stays valid until the cache is invalidated.
		// Empty on failure
		[[nodiscard]] std::span<const byte> GetShader(std::string_view virtualPathNoExt, std::span<const ShaderDefine> defines = {});

		// Drops the entries whose source or any included header changed on disk since they
		// were compiled, leaving the rest alone. Returns the virtual paths dropped, so
		// callers can rebuild just what depended on them
		std::vector<std::string> InvalidateStale();

		// Drops every entry, so the next GetShader recompiles
		void InvalidateAll();

	private:
		struct Dependency
		{
			fs::path           Path;
			fs::file_time_type Stamp{};
		};

		struct Entry
		{
			std::vector<byte>       Bytecode;
			std::vector<Dependency> Dependencies;  // the source itself, then everything it includes
		};

		ShaderCache() = default;

		[[nodiscard]] bool LoadBytecode(const ShaderKey& key, std::span<const ShaderDefine> defines, Entry& outEntry) const;

		// The first dependency that no longer matches its recorded timestamp, or null if none
		[[nodiscard]] static const fs::path* FindChangedDependency(const Entry& entry);

		static void AddDependency(Entry& entry, const fs::path& path);

	private:
		std::unordered_map<ShaderKey, Entry, ShaderKey::Hash> m_entries;
	};
}
