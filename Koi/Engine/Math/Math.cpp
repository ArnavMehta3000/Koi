#include "Koi/Engine/Math/Math.h"
#include "Koi/Engine/ECS/World.h"
#include <algorithm>
#include <cmath>
#include <array>

namespace koi
{
	DecomposedTransform MathUtils::DecomposeTransform(const float4x4& m)
	{
		DecomposedTransform result;

		std::get<0>(result) = m[3].xyz;

		std::array rows{ m[0].xyz, m[1].xyz, m[2].xyz };
		std::array scale{ length(rows[0]), length(rows[1]), length(rows[2]) };

		if (static_cast<f32>(determinant(float3x3(rows[0], rows[1], rows[2]))) < 0.0f)
			scale[0] = -scale[0];

		std::get<2>(result) = float3(scale[0], scale[1], scale[2]);

		for (i32 i = 0; i < 3; ++i)
			rows[i] = rows[i] / scale[i];

		std::get<1>(result) = quaternion(float3x3(rows[0], rows[1], rows[2]));
		return result;
	}

	void MathUtils::RegisterHLSLPPTypes(World& world)
	{
		using namespace std::string_view_literals;
		auto& ecs = world.GetECS();

		// float2
		ecs.component<float2>("float2")
			.opaque(ecs.component()
				.member<f32>("x").member<f32>("y"))
			.serialize([](const flecs::serializer* s, const float2* v)
			{
				interop::float2 e(*v);
				s->member("x"); s->value(e.x);
				s->member("y"); s->value(e.y);
				return 0;
			})
			.ensure_member([](float2* dst, const char* member) -> void*
			{
				if (member == "x"sv) return &dst->x;
				if (member == "y"sv) return &dst->y;
				return nullptr;
			});

		// float 3
		ecs.component<float3>("float3")
			.opaque(ecs.component()
				.member<f32>("x").member<f32>("y").member<f32>("z"))
			.serialize([](const flecs::serializer* s, const float3* v)
			{
				interop::float3 e(*v);
				s->member("x"); s->value(e.x);
				s->member("y"); s->value(e.y);
				s->member("z"); s->value(e.z);
				return 0;
			})
			.ensure_member([](float3* dst, const char* member) -> void*
			{
				if (member == "x"sv) return &dst->x;
				if (member == "y"sv) return &dst->y;
				if (member == "z"sv) return &dst->z;
				return nullptr;
			});

		// float 4
		ecs.component<float4>("float4")
			.opaque(ecs.component()
				.member<f32>("x").member<f32>("y").member<f32>("z").member<f32>("w"))
			.serialize([](const flecs::serializer* s, const float4* v)
			{
				interop::float4 e(*v);
				s->member("x"); s->value(e.x);
				s->member("y"); s->value(e.y);
				s->member("z"); s->value(e.z);
				s->member("w"); s->value(e.w);
				return 0;
			})
			.ensure_member([](float4* dst, const char* member) -> void*
			{
				if (member == "x"sv) return &dst->x;
				if (member == "y"sv) return &dst->y;
				if (member == "z"sv) return &dst->z;
				if (member == "w"sv) return &dst->w;
				return nullptr;
			});

		// quaternion
		ecs.component<quaternion>("quaternion")
			.opaque(ecs.component()
				.member<f32>("x").member<f32>("y").member<f32>("z").member<f32>("w"))
			.serialize([](const flecs::serializer* s, const quaternion* v)
			{
				interop::float4 e{};
				store(&e.x, *v);  // Store the quaternion as a float4

				s->member("x"); s->value(e.x);
				s->member("y"); s->value(e.y);
				s->member("z"); s->value(e.z);
				s->member("w"); s->value(e.w);
				return 0;
			})
			.ensure_member([](quaternion* dst, const char* member) -> void*
			{
				if (member == "x"sv) return &dst->x;
				if (member == "y"sv) return &dst->y;
				if (member == "z"sv) return &dst->z;
				if (member == "w"sv) return &dst->w;
				return nullptr;
			});

		// TODO: Other math types
	}

	void AABB::Expand(const float3 point)
	{
		Min = min(Min, point);
		Max = max(Max, point);
	}

	void AABB::Expand(const AABB& other)
	{
		Expand(other.Min);
		Expand(other.Max);
	}

	[[nodiscard]] float3 AABB::Center() const
	{
		return (Min + Max) * 0.5f;
	}

	// Radius of a sphere that encloses this AABB, centered at Center()
	[[nodiscard]] f32 AABB::Radius() const
	{
		const float3 extent = (Max - Min) * 0.5f;
		f32 e[3]{};
		store(e, extent);
		return std::max(std::sqrt(e[0] * e[0] + e[1] * e[1] + e[2] * e[2]), 0.001f);
	}
}
