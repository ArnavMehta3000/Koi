#pragma once
#include "Koi/Engine/Core/Types.h"
#include "Koi/Engine/Runtime/Serializer.h"

namespace koi
{
	inline constexpr std::string_view kProceduralScheme = "proc://";

	// Canonical form is "proc://<generator>?key=value&...", keys sorted (toml::table iterates
	// sorted), so two specs authored in different key orders share one AssetCache entry.
	[[nodiscard]] std::string ToProceduralUri(std::string_view generator, const toml::table& params);
	[[nodiscard]] std::optional<std::pair<std::string, toml::table>> ParseProceduralUri(std::string_view uri);
}
