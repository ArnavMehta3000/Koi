#pragma once
#include <string>

namespace koi
{
	// An asset is identified by its virtual path
	class AssetKey
	{
	public:
		AssetKey() = default;
		explicit AssetKey(std::string path) : m_path(std::move(path)) {}

		[[nodiscard]] bool IsValid() const { return !m_path.empty(); }
		[[nodiscard]] const std::string& Path() const { return m_path; }
		[[nodiscard]] const std::string& ToString() const { return m_path; }

		explicit operator bool() const { return IsValid(); }

		bool operator==(const AssetKey&) const = default;

	private:
		std::string m_path;
	};
}

template<>
struct std::hash<koi::AssetKey>
{
	size_t operator()(const koi::AssetKey& key) const noexcept
	{
		return std::hash<std::string>{}(key.Path());
	}
};
