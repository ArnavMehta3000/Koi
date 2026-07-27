#pragma once
#include "Koi/Engine/ECS/Components.h"
#include "Koi/Engine/Runtime/Serializer.h"

namespace koi
{
	template<>
	struct Serializer<CameraComponent>
	{
		static toml::table Serialize(const CameraComponent& c)
		{
			toml::table out;
			out.insert("fov", static_cast<f64>(c.FovY));
			out.insert("near", static_cast<f64>(c.Near));
			out.insert("far", static_cast<f64>(c.Far));
			out.insert("primary", c.Primary);
			return out;
		}

		static std::optional<CameraComponent> Deserialize(const toml::node& node)
		{
			const toml::table* tbl = node.as_table();
			if (!tbl) return std::nullopt;

			return CameraComponent
			{
				.FovY    = (*tbl)["fov"].value_or(0.8f),
				.Near    = (*tbl)["near"].value_or(0.05f),
				.Far     = (*tbl)["far"].value_or(1000.0f),
				.Primary = (*tbl)["primary"].value_or(true),
			};
		}
	};

	static_assert(Serializable<CameraComponent>);
}
