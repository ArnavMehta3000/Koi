#pragma once
#include "Koi/Engine/RHI/Types.h"
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

	enum class ShaderBuildState : u8
	{
		Missing,   // not in any Shaders.json
		Stale,     // a source or include changed since xmake last built it
		UpToDate,
	};

	// Owns the bytecode of every shader variant loaded, keyed by source path, stage and defines
	// Prebuilt variants come from <scheme>://Shaders/Manifest.toml (written by the KoiShaders xmake rule)
	// Where the .hlsl is on disk too (the editor), a stale or unlisted shader is compiled on the spot
	class ShaderCache : public NonCopyable
	{
	public:
		static ShaderCache& Get();

		// *VS -> Vertex, *PS -> Pixel, etc
		[[nodiscard]] static ShaderType InferType(std::string_view path);

		// Sorts by name first
		[[nodiscard]] static u64 HashDefines(std::span<const ShaderDefine> defines);

		// Bytecode for a virtual path WITHOUT extension ("engine://Content/Shaders/MeshVS").
		// The span points into cache storage and stays valid until the cache is invalidated.
		// Empty on failure
		[[nodiscard]] std::span<const byte> GetShader(std::string_view virtualPathNoExt, std::span<const ShaderDefine> defines = {});

		// Reflection of the same variant. Zeroed when it failed to load
		[[nodiscard]] ShaderInfo GetInfo(std::string_view virtualPathNoExt, std::span<const ShaderDefine> defines = {});

		// Drops the entries whose source or any included header changed on disk since they
		// were compiled, leaving the rest alone. Returns the virtual paths dropped, so
		// callers can rebuild just what depended on them
		std::vector<std::string> InvalidateStale();

		// Whether any variant of this shader is currently compiled. Path carries no extension
		[[nodiscard]] bool IsCompiled(std::string_view virtualPathNoExt) const;

		// Drops every entry, so the next GetShader recompiles
		void InvalidateAll();

		// How the build output compares to the sources, across every variant of this shader
		[[nodiscard]] ShaderBuildState GetBuildState(std::string_view virtualPathNoExt);

	private:
		struct Dependency
		{
			fs::path           Path;
			fs::file_time_type Stamp{};
		};

		struct Entry
		{
			std::vector<byte>       Bytecode;
			ShaderInfo              Info;
			std::vector<Dependency> Dependencies;  // the source itself, then everything it includes
		};

		// One record of a Manifest.toml, all paths virtual
		struct BuiltShader
		{
			std::string              Bytecode;
			std::string              Reflection;
			std::vector<std::string> Dependencies;  // the source, then its includes
		};

		ShaderCache() = default;

		void LoadManifests();
		[[nodiscard]] const Entry* Find(std::string_view virtualPathNoExt, std::span<const ShaderDefine> defines);

		[[nodiscard]] bool LoadBytecode(const ShaderKey& key, std::span<const ShaderDefine> defines, Entry& outEntry) const;
		[[nodiscard]] bool LoadBuilt(const BuiltShader& built, bool trackDependencies, Entry& outEntry) const;
		[[nodiscard]] bool Compile(const ShaderKey& key, std::span<const ShaderDefine> defines, const fs::path& source, Entry& outEntry) const;

		[[nodiscard]] static bool IsStale(const BuiltShader& built);

		// The first dependency that no longer matches its recorded timestamp, or null if none
		[[nodiscard]] static const fs::path* FindChangedDependency(const Entry& entry);

		static void AddDependency(Entry& entry, const fs::path& path);

	private:
		std::unordered_map<ShaderKey, Entry, ShaderKey::Hash>       m_entries;
		std::unordered_map<ShaderKey, BuiltShader, ShaderKey::Hash> m_built;
		bool                                                        m_manifestsLoaded = false;
	};
}
