#include "Koi/Engine/IO/VirtualFileSystem.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Logging/Logger.h"
#include <fstream>

namespace koi
{
	FilesystemMount::FilesystemMount(const fs::path& root) : m_roots{ std::move(root) } { }

	FilesystemMount::FilesystemMount(std::span<fs::path> roots)
	{
		m_roots.assign(roots.begin(), roots.end());
	}

	std::optional<std::vector<byte>> FilesystemMount::Read(std::string_view relativePath)
	{
		ZoneScoped;

		for (const fs::path& root : m_roots)
		{
			fs::path full = root / fs::path(relativePath);
			std::ifstream file(full, std::ios::binary | std::ios::ate);
			if (!file.is_open()) continue;

			std::vector<byte> buffer(static_cast<size_t>(file.tellg()));
			file.seekg(0, std::ios::beg);
			file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());

			KOI_LOG(IO, Trace, "VFS read '{}' -> '{}' ({} bytes)", relativePath, full.string(), buffer.size());
			return buffer;
		}

		KOI_LOG(IO, Trace, "VFS read '{}' missing in all {} root(s)", relativePath, m_roots.size());
		return std::nullopt;
	}

	std::optional<fs::path> FilesystemMount::TryResolveToDisk(std::string_view relativePath)
	{
		KOI_ZONE_UNMUTED;  // Texture decode can arrive here from a parallel-for loop

		// Return the first root that actually contains the file
		for (const fs::path& root : m_roots)
		{
			fs::path full = root / fs::path(relativePath);
			if (fs::exists(full)) return full;
		}

		return m_roots.empty()
			? std::optional<fs::path>{}  // No roots, no file
			: (m_roots.front() / fs::path(relativePath));
	}

	VirtualFileSystem& VirtualFileSystem::Get()
	{
		static VirtualFileSystem instance;
		return instance;
	}

	void VirtualFileSystem::Mount(const std::string& scheme, std::unique_ptr<IMountBackend> backend)
	{
		KOI_LOG(IO, Debug, "VFS mount '{}://' registered", scheme);
		m_mounts[std::move(scheme)] = std::move(backend);
	}

	void VirtualFileSystem::Unmount(std::string_view scheme)
	{
		KOI_LOG(IO, Debug, "VFS mount '{}://' removed", scheme);
		m_mounts.erase(std::string(scheme));
	}

	std::optional<std::pair<std::string_view, std::string_view>>
		VirtualFileSystem::SplitVirtualPath(std::string_view virtualPath)
	{
		const auto pos = virtualPath.find("://");

		if (pos == std::string_view::npos)
			return std::nullopt;

		return std::make_pair(virtualPath.substr(0, pos), virtualPath.substr(pos + 3));
	}

	namespace
	{
		// Paths without a scheme are implicitly project://. Assets in user-authored
		// TOMLs are project assets all the time, so the prefix is noise; engine://
		// stays opt-in for the rare cross-reference.
		std::pair<std::string_view, std::string_view> ResolveScheme(std::string_view virtualPath)
		{
			if (auto split = VirtualFileSystem::SplitVirtualPath(virtualPath))
				return *split;
			return { std::string_view{"project"}, virtualPath };
		}
	}

	std::optional<std::vector<byte>> VirtualFileSystem::Read(std::string_view virtualPath)
	{
		const auto [scheme, path] = ResolveScheme(virtualPath);

		auto it = m_mounts.find(std::string(scheme));
		if (it == m_mounts.end())
		{
			KOI_LOG(IO, Error, "VFS Read: scheme '{}://' not mounted (path '{}')", scheme, virtualPath);
			return std::nullopt;
		}

		return it->second->Read(path);
	}

	std::optional<fs::path> VirtualFileSystem::TryResolveToDisk(std::string_view virtualPath)
	{
		const auto [scheme, path] = ResolveScheme(virtualPath);

		auto it = m_mounts.find(std::string(scheme));
		if (it == m_mounts.end())
		{
			KOI_LOG(IO, Trace, "VFS resolve: scheme '{}://' not mounted (path '{}')", scheme, virtualPath);
			return std::nullopt;
		}

		return it->second->TryResolveToDisk(path);
	}

	void VirtualFileSystem::ForEachDiskRoot(const std::function<void(std::string_view scheme, const fs::path& root)>& visit) const
	{
		for (const auto& [scheme, backend] : m_mounts)
		{
			for (const fs::path& root : backend->GetRoots())
			{
				visit(scheme, root);
			}
		}
	}
}
