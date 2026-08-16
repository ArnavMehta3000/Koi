#pragma once
#include "Koi/Engine/Math/Transform.h"
#include "Koi/Engine/Math/MathSerializer.h"

namespace koi
{
	template<>
	struct Serializer<Transform>
	{
		static toml::table Serialize(const Transform& t)
		{
			toml::table out;
			SerializeInto(out, "position", t.Position);
			SerializeInto(out, "rotation", t.Rotation);
			SerializeInto(out, "scale", t.Scale);
			return out;
		}

		static std::optional<Transform> Deserialize(const toml::node& node)
		{
			using namespace std::string_view_literals;

			if (const toml::table* asTable = node.as_table();
				asTable->contains("position")
				&& asTable->contains("rotation")
				&& asTable->contains("scale"))
			{
				const toml::table& tbl = *asTable;

				return Transform
				{
					.Position = DeserializeOr(tbl, "position", Vector3(0.0f, 0.0f, 0.0f)),
					.Rotation = DeserializeOr(tbl, "rotation", Quaternion::Identity),
					.Scale    = DeserializeOr(tbl, "scale", Vector3(1.0f, 1.0f, 1.0f))
				};
			}
			return std::nullopt;
		}
	};

	static_assert(Serializable<Transform>);
}
