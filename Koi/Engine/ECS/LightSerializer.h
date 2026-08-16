#pragma once
#include "Koi/Engine/ECS/Components.h"
#include "Koi/Engine/Math/MathSerializer.h"

namespace koi
{
	template<>
	struct EnumNames<LightType>
	{
		static std::string_view ToString(LightType value)
		{
			switch (value)
			{
				case LightType::Directional: return "directional";
				case LightType::Point:       return "point";
			}
			return "point";
		}

		static std::optional<LightType> FromString(std::string_view name)
		{
			if (name == "directional") return LightType::Directional;
			if (name == "point")       return LightType::Point;
			return std::nullopt;
		}
	};

	template<>
	struct Serializer<Light>
	{
		static toml::table Serialize(const Light& l)
		{
			toml::table out;
			SerializeInto(out, "type", l.Type);
			SerializeInto(out, "color", l.Color);
			out.insert("intensity", static_cast<f64>(l.Intensity));
			out.insert("range", static_cast<f64>(l.Range));
			out.insert("casts_shadows", l.CastsShadows);
			out.insert("shadow_bias", static_cast<f64>(l.ShadowBias));
			return out;
		}

		static std::optional<Light> Deserialize(const toml::node& node)
		{
			const toml::table* tbl = node.as_table();
			if (!tbl) return std::nullopt;

			return Light
			{
				.Type      = DeserializeOr(*tbl, "type", LightType::Point),
				.Color     = DeserializeOr(*tbl, "color", Vector3(1.0f, 1.0f, 1.0f)),
				.Intensity = (*tbl)["intensity"].value_or(1.0f),
				.Range     = (*tbl)["range"].value_or(10.0f),

				.CastsShadows = (*tbl)["casts_shadows"].value_or(false),
				.ShadowBias   = (*tbl)["shadow_bias"].value_or(0.0015f),
			};
		}
	};

	static_assert(Serializable<Light>);
}
