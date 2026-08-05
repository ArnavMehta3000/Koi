#pragma once
#include "Koi/Engine/Core/Types.h"
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <span>

namespace koi
{
	namespace fs = std::filesystem;

	// Mounts are referred to by schemes & resolve relative virtual paths into byte stream or disk path
	class IMountBackend
	{
	public:
		virtual ~IMountBackend() = default;

		// Read the entire contents of a virtual file relative to this mount
		[[nodiscard]] virtual std::optional<std::vector<byte>> Read(std::string_view relativePath) = 0;

		// Resolve a virtual path to a concrete disk path (if using a filesystem mount)
		[[nodiscard]] virtual std::optional<fs::path> TryResolveToDisk(std::string_view relativePath) = 0;
	};

	class FilesystemMount : public IMountBackend
	{
	public:
		explicit FilesystemMount(const fs::path& root);
		explicit FilesystemMount(std::span<fs::path> roots);

		[[nodiscard]] inline std::span<const fs::path> GetRoots() const { return m_roots; }

		[[nodiscard]] std::optional<std::vector<byte>> Read(std::string_view relativePath) override;
		[[nodiscard]] std::optional<fs::path> TryResolveToDisk(std::string_view relativePath) override;

	private:
		std::vector<fs::path> m_roots;
	};

	class VirtualFileSystem
	{
	public:
		[[nodiscard]] static VirtualFileSystem& Get();

		[[nodiscard]] static std::optional<std::pair<std::string_view, std::string_view>> SplitVirtualPath(std::string_view virtualPath);

		// Mounts are referred to by schemes ("scheme:://<relative/path>")
		void Mount(const std::string& scheme, std::unique_ptr<IMountBackend> backend);
		void Unmount(std::string_view scheme);

		[[nodiscard]] std::optional<std::vector<byte>> Read(std::string_view virtualPath);
		[[nodiscard]] std::optional<fs::path> TryResolveToDisk(std::string_view virtualPath);

	private:
		std::unordered_map<std::string, std::unique_ptr<IMountBackend>> m_mounts;
	};
}
