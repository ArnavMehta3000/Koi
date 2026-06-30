module;
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <span>
export module Koi.Core.VirtualFileSystem;

import Koi.Core.Types;

export namespace koi
{
	namespace fs = std::filesystem;

	class IMountBackend
	{
	public:
		virtual ~IMountBackend() = default;
		[[nodiscard]] virtual std::optional<std::vector<byte>> Read(std::string_view relativePath) = 0;
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

		void Mount(const std::string& scheme, std::unique_ptr<IMountBackend> backend);
		void Unmount(std::string_view scheme);

		[[nodiscard]] std::optional<std::vector<byte>>  Read(std::string_view virtualPath);
		[[nodiscard]] std::optional<fs::path> TryResolveToDisk(std::string_view virtualPath);

	private:
		std::unordered_map<std::string, std::unique_ptr<IMountBackend>> m_mounts;
	};
}
