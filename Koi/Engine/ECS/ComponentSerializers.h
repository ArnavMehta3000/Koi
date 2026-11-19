#pragma once
#include "Koi/Engine/ECS/Components.h"
#include "Koi/Engine/Core/Serializer.h"

namespace koi
{
	template<>
	struct Serializer<MeshRef>
	{
		static toml::table Serialize(const MeshRef& mesh);
		static std::optional<MeshRef> Deserialize(const toml::node& node);
	};

	template<>
	struct Serializer<MaterialRef>
	{
		static toml::table Serialize(const MaterialRef& material);
		static std::optional<MaterialRef> Deserialize(const toml::node& node);
	};
}
