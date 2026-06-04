#include "Core/VirtualFileSystem.h"
#include "Core/Logger.h"
#include "Core/Profiling.h"
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
			return buffer;
		}
		return std::nullopt;
	}

	std::optional<fs::path> FilesystemMount::TryResolveToDisk(std::string_view relativePath)
	{
		ZoneScoped;

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
		KOI_LOG_INFO("VFS mount '{}://' registered", scheme);
		m_mounts[std::move(scheme)] = std::move(backend);
	}

	void VirtualFileSystem::Unmount(std::string_view scheme)
	{
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

	std::optional<std::vector<byte>> VirtualFileSystem::Read(std::string_view virtualPath)
	{
		auto split = SplitVirtualPath(virtualPath);
		if (!split)
		{
			KOI_LOG_ERROR("VFS Read: '{}' has no scheme prefix", virtualPath);
			return std::nullopt;
		}

		const auto& [scheme, path] = *split;

		auto it = m_mounts.find(std::string(scheme));
		if (it == m_mounts.end())
		{
			KOI_LOG_ERROR("VFS Read: scheme '{}://' not mounted (path '{}')", scheme, virtualPath);
			return std::nullopt;
		}

		return it->second->Read(split->second);
	}

	std::optional<fs::path> VirtualFileSystem::TryResolveToDisk(std::string_view virtualPath)
	{
		auto split = SplitVirtualPath(virtualPath);
		if (!split) return std::nullopt;

		const auto& [scheme, path] = *split;

		auto it = m_mounts.find(std::string(scheme));
		if (it == m_mounts.end()) return std::nullopt;

		return it->second->TryResolveToDisk(path);
	}
}
