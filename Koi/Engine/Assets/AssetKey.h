#pragma once
#include "Koi/Engine/Core/UUID.h"
#include <string_view>
#include <variant>

namespace koi
{
	class AssetKey
	{
	public:
		AssetKey() = default;
		explicit AssetKey(const UUID& id) : m_value(id) {}

		[[nodiscard]] static AssetKey FromUri(std::string uri);
		[[nodiscard]] static AssetKey Parse(std::string_view text);

		[[nodiscard]] bool IsValid() const;
		[[nodiscard]] bool IsUri() const { return std::holds_alternative<std::string>(m_value); }

		// Nil / empty when the key holds the other alternative
		[[nodiscard]] const UUID&      Id()  const;
		[[nodiscard]] std::string_view Uri() const;

		[[nodiscard]] std::string ToString() const;

		explicit operator bool() const { return IsValid(); }

		bool operator==(const AssetKey&) const = default;

	private:
		std::variant<UUID, std::string> m_value;
	};
}

template<>
struct std::hash<koi::AssetKey>
{
	size_t operator()(const koi::AssetKey& key) const noexcept
	{
		return key.IsUri()
			? std::hash<std::string_view>{}(key.Uri())
			: std::hash<koi::UUID>{}(key.Id());
	}
};
