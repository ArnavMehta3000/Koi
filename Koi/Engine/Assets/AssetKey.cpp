#include "Koi/Engine/Assets/AssetKey.h"
#include "Koi/Engine/Core/Patterns/Match.h"

namespace koi
{
	AssetKey AssetKey::FromUri(std::string uri)
	{
		AssetKey key;
		key.m_value = std::move(uri);
		return key;
	}

	AssetKey AssetKey::Parse(std::string_view text)
	{
		if (text.empty()) return {};

		if (const UUID id = UUIDFromString(text); !id.is_nil()) return AssetKey(id);

		return FromUri(std::string(text));
	}

	bool AssetKey::IsValid() const
	{
		return std::visit(Match
		{
			[](const UUID& id)         { return !id.is_nil(); },
			[](const std::string& uri) { return !uri.empty(); },
		}, m_value);
	}

	const UUID& AssetKey::Id() const
	{
		static const UUID kNil{};
		const UUID* id = std::get_if<UUID>(&m_value);
		return id ? *id : kNil;
	}

	std::string_view AssetKey::Uri() const
	{
		const std::string* uri = std::get_if<std::string>(&m_value);
		return uri ? std::string_view(*uri) : std::string_view();
	}

	std::string AssetKey::ToString() const
	{
		return IsUri() ? std::string(Uri()) : koi::ToString(Id());
	}
}
