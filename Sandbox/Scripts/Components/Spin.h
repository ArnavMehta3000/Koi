#pragma once
#include "Koi/Engine/Interop/ComponentInspector.h"
#include "Koi/Engine/Math/MathSerializer.h"

// A component the engine has never heard of: defined, serialized, inspected and
// simulated entirely from game code
struct Spin
{
	koi::Vector3 Axis  = koi::Vector3(0.0f, 1.0f, 0.0f);
	koi::f32     Speed = 90.0f;  // degrees per second, about Axis in world space
};

namespace koi
{
	template<>
	struct Serializer<Spin>
	{
		static toml::table Serialize(const Spin& s)
		{
			toml::table out;
			SerializeInto(out, "axis", s.Axis);
			out.insert("speed", ToTomlFloat(s.Speed));
			return out;
		}

		static std::optional<Spin> Deserialize(const toml::node& node)
		{
			const toml::table* tbl = node.as_table();
			if (!tbl) return std::nullopt;

			return Spin
			{
				.Axis  = DeserializeOr(*tbl, "axis", Vector3(0.0f, 1.0f, 0.0f)),
				.Speed = (*tbl)["speed"].value_or(90.0f),
			};
		}
	};

	static_assert(Serializable<Spin>);
}

class SpinInspector final : public koi::IComponentInspector
{
public:
	[[nodiscard]] std::string_view Key() const override { return "spin"; }
	void Draw(koi::Entity e) override;
};

void RegisterSpinComponent();
void RegisterSpinSystem(koi::ECS& ecs);
