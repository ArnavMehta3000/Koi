#pragma once
#include "Koi/Engine/Graphics/Procedural/MeshBuilder.h"
#include "Koi/Engine/Graphics/Procedural/ProceduralMeshSpec.h"
#include "Koi/Engine/Math/MathSerializer.h"
#include "Koi/Engine/Runtime/Subsystems.h"  // For TypeList
#include <expected>
#include <memory>
#include <span>

namespace koi
{
	template<typename G>
	concept MeshGeneratorType = requires
	{
	    { G::Name } -> std::convertible_to<std::string_view>;

		// The generator should have a params struct that is serializable & default constructable
	    typename G::Params;
	    requires Serializable<typename G::Params>;
	    requires std::default_initializable<typename G::Params>;

		{ G::Generate(std::declval<MeshBuilder&>(), std::declval<const typename G::Params&>()) } -> std::same_as<void>;
	};

	// Clamp is optional: a generator with no unsafe parameter range simply omits it
	template<MeshGeneratorType G>
	void ClampParams(typename G::Params& params)
	{
		if constexpr (requires { G::Clamp(params); })
		{
			G::Clamp(params);
		}
	}

	// --- Primitives ---

	struct PlaneGenerator
	{
		static constexpr std::string_view Name = "plane";

		struct Params
		{
			Vector2 Size      = Vector2::One;  // extent on X and Z
			i32     SegmentsX = 1;
			i32     SegmentsZ = 1;
		};

		static void Generate(MeshBuilder& builder, const Params& p);
		static void Clamp(Params& p);
	};

	struct BoxGenerator
	{
		static constexpr std::string_view Name = "box";

		struct Params
		{
			Vector3 Size = Vector3::One;
			i32     Segments = 1;
		};

		static void Generate(MeshBuilder& builder, const Params& p);
		static void Clamp(Params& p);
	};

	struct SphereGenerator
	{
		static constexpr std::string_view Name = "sphere";

		struct Params
		{
			f32 Radius   = 0.5f;
			i32 Segments = 32;   // longitude
			i32 Rings    = 16;   // latitude
		};

		static void Generate(MeshBuilder& builder, const Params& p);
		static void Clamp(Params& p);
	};

	struct CylinderGenerator
	{
		static constexpr std::string_view Name = "cylinder";

		struct Params
		{
			f32  Radius   = 0.5f;
			f32  Height   = 1.0f;
			i32  Segments = 32;
			bool Caps     = true;
		};

		static void Generate(MeshBuilder& builder, const Params& p);
		static void Clamp(Params& p);
	};

	struct ConeGenerator
	{
		static constexpr std::string_view Name = "cone";

		struct Params
		{
			f32  Radius   = 0.5f;
			f32  Height   = 1.0f;
			i32  Segments = 32;
			bool Cap      = true;
		};

		static void Generate(MeshBuilder& builder, const Params& p);
		static void Clamp(Params& p);
	};

	struct CapsuleGenerator
	{
		static constexpr std::string_view Name = "capsule";

		struct Params
		{
			f32 Radius   = 0.5f;
			f32 Height   = 1.0f;   // cylindrical section only, total is Height + 2 * Radius
			i32 Segments = 32;
			i32 Rings    = 8;      // per hemisphere
		};

		static void Generate(MeshBuilder& builder, const Params& p);
		static void Clamp(Params& p);
	};

	struct TorusGenerator
	{
		static constexpr std::string_view Name = "torus";

		struct Params
		{
			f32 Radius     = 0.5f;
			f32 TubeRadius = 0.2f;
			i32 Segments   = 32;   // around the major radius
			i32 Sides      = 16;   // around the tube
		};

		static void Generate(MeshBuilder& builder, const Params& p);
		static void Clamp(Params& p);
	};

	// --- Params serialization ---

	template<>
	struct Serializer<PlaneGenerator::Params>
	{
		using P = PlaneGenerator::Params;

		static toml::table Serialize(const P& p)
		{
			toml::table out;
			SerializeInto(out, "size", p.Size);
			out.insert("segments_x", static_cast<i64>(p.SegmentsX));
			out.insert("segments_z", static_cast<i64>(p.SegmentsZ));
			return out;
		}

		static std::optional<P> Deserialize(const toml::node& node)
		{
			const toml::table* tbl = node.as_table();
			if (!tbl) return std::nullopt;

			return P
			{
				.Size      = DeserializeOr(*tbl, "size", Vector2::One),
				.SegmentsX = (*tbl)["segments_x"].value_or(1),
				.SegmentsZ = (*tbl)["segments_z"].value_or(1),
			};
		}
	};

	template<>
	struct Serializer<BoxGenerator::Params>
	{
		using P = BoxGenerator::Params;

		static toml::table Serialize(const P& p)
		{
			toml::table out;
			SerializeInto(out, "size", p.Size);
			out.insert("segments", static_cast<i64>(p.Segments));
			return out;
		}

		static std::optional<P> Deserialize(const toml::node& node)
		{
			const toml::table* tbl = node.as_table();
			if (!tbl) return std::nullopt;

			return P
			{
				.Size     = DeserializeOr(*tbl, "size", Vector3::One),
				.Segments = (*tbl)["segments"].value_or(1),
			};
		}
	};

	template<>
	struct Serializer<SphereGenerator::Params>
	{
		using P = SphereGenerator::Params;

		static toml::table Serialize(const P& p)
		{
			toml::table out;
			out.insert("radius", ToTomlFloat(p.Radius));
			out.insert("segments", static_cast<i64>(p.Segments));
			out.insert("rings", static_cast<i64>(p.Rings));
			return out;
		}

		static std::optional<P> Deserialize(const toml::node& node)
		{
			const toml::table* tbl = node.as_table();
			if (!tbl) return std::nullopt;

			return P
			{
				.Radius   = (*tbl)["radius"].value_or(0.5f),
				.Segments = (*tbl)["segments"].value_or(32),
				.Rings    = (*tbl)["rings"].value_or(16),
			};
		}
	};

	template<>
	struct Serializer<CylinderGenerator::Params>
	{
		using P = CylinderGenerator::Params;

		static toml::table Serialize(const P& p)
		{
			toml::table out;
			out.insert("radius", ToTomlFloat(p.Radius));
			out.insert("height", ToTomlFloat(p.Height));
			out.insert("segments", static_cast<i64>(p.Segments));
			out.insert("caps", p.Caps);
			return out;
		}

		static std::optional<P> Deserialize(const toml::node& node)
		{
			const toml::table* tbl = node.as_table();
			if (!tbl) return std::nullopt;

			return P
			{
				.Radius   = (*tbl)["radius"].value_or(0.5f),
				.Height   = (*tbl)["height"].value_or(1.0f),
				.Segments = (*tbl)["segments"].value_or(32),
				.Caps     = (*tbl)["caps"].value_or(true),
			};
		}
	};

	template<>
	struct Serializer<ConeGenerator::Params>
	{
		using P = ConeGenerator::Params;

		static toml::table Serialize(const P& p)
		{
			toml::table out;
			out.insert("radius", ToTomlFloat(p.Radius));
			out.insert("height", ToTomlFloat(p.Height));
			out.insert("segments", static_cast<i64>(p.Segments));
			out.insert("cap", p.Cap);
			return out;
		}

		static std::optional<P> Deserialize(const toml::node& node)
		{
			const toml::table* tbl = node.as_table();
			if (!tbl) return std::nullopt;

			return P
			{
				.Radius   = (*tbl)["radius"].value_or(0.5f),
				.Height   = (*tbl)["height"].value_or(1.0f),
				.Segments = (*tbl)["segments"].value_or(32),
				.Cap      = (*tbl)["cap"].value_or(true),
			};
		}
	};

	template<>
	struct Serializer<CapsuleGenerator::Params>
	{
		using P = CapsuleGenerator::Params;

		static toml::table Serialize(const P& p)
		{
			toml::table out;
			out.insert("radius", ToTomlFloat(p.Radius));
			out.insert("height", ToTomlFloat(p.Height));
			out.insert("segments", static_cast<i64>(p.Segments));
			out.insert("rings", static_cast<i64>(p.Rings));
			return out;
		}

		static std::optional<P> Deserialize(const toml::node& node)
		{
			const toml::table* tbl = node.as_table();
			if (!tbl) return std::nullopt;

			return P
			{
				.Radius   = (*tbl)["radius"].value_or(0.5f),
				.Height   = (*tbl)["height"].value_or(1.0f),
				.Segments = (*tbl)["segments"].value_or(32),
				.Rings    = (*tbl)["rings"].value_or(8),
			};
		}
	};

	template<>
	struct Serializer<TorusGenerator::Params>
	{
		using P = TorusGenerator::Params;

		static toml::table Serialize(const P& p)
		{
			toml::table out;
			out.insert("radius", ToTomlFloat(p.Radius));
			out.insert("tube_radius", ToTomlFloat(p.TubeRadius));
			out.insert("segments", static_cast<i64>(p.Segments));
			out.insert("sides", static_cast<i64>(p.Sides));
			return out;
		}

		static std::optional<P> Deserialize(const toml::node& node)
		{
			const toml::table* tbl = node.as_table();
			if (!tbl) return std::nullopt;

			return P
			{
				.Radius     = (*tbl)["radius"].value_or(0.5f),
				.TubeRadius = (*tbl)["tube_radius"].value_or(0.2f),
				.Segments   = (*tbl)["segments"].value_or(32),
				.Sides      = (*tbl)["sides"].value_or(16),
			};
		}
	};

	using Primitives = TypeList<PlaneGenerator, BoxGenerator, SphereGenerator, CylinderGenerator, ConeGenerator, CapsuleGenerator, TorusGenerator>;

	static_assert(MeshGeneratorType<PlaneGenerator>);
	static_assert(MeshGeneratorType<BoxGenerator>);
	static_assert(MeshGeneratorType<SphereGenerator>);
	static_assert(MeshGeneratorType<CylinderGenerator>);
	static_assert(MeshGeneratorType<ConeGenerator>);
	static_assert(MeshGeneratorType<CapsuleGenerator>);
	static_assert(MeshGeneratorType<TorusGenerator>);

	// --- Registry ---

	struct MeshGeneratorInfo
	{
		using GenerateFn = std::expected<ModelAsset, std::string>(*)(const toml::table&);
		using DefaultsFn = toml::table(*)();
		using ClampFn    = void(*)(toml::table&);

		std::string Name;
		GenerateFn Generate = nullptr;
		DefaultsFn Defaults = nullptr;
		ClampFn    Clamp    = nullptr;   // Also normalises the table's key set and types
	};

	class MeshGeneratorRegistry
	{
	public:
		[[nodiscard]] static MeshGeneratorRegistry& Get();

		template<MeshGeneratorType G>
		void Register()
		{
			Register(MeshGeneratorInfo
			{
				.Name = std::string(G::Name),

				.Generate = [](const toml::table& params) -> std::expected<ModelAsset, std::string>
				{
					using P = typename G::Params;

					// A missing or malformed table degrades to defaults instead of failing the load,
					// and clamping here means a hand-authored or scripted spec is as safe as the editor changes
					P typed = Serializer<P>::Deserialize(params).value_or(P{});
					ClampParams<G>(typed);

					MeshBuilder builder;
					G::Generate(builder, typed);

					// Guards GPUMesh::Upload, which asserts on an empty vertex or index buffer
					if (builder.IsEmpty())
						return std::unexpected(std::string(G::Name) + " produced no geometry");

					return std::move(builder).Build(std::string(G::Name));
				},

				.Defaults = [] { return Serializer<typename G::Params>::Serialize(typename G::Params{}); },

				.Clamp = [](toml::table& params)
				{
					using P = typename G::Params;

					P typed = Serializer<P>::Deserialize(params).value_or(P{});
					ClampParams<G>(typed);
					params = Serializer<P>::Serialize(typed);
				}
			});
		}

		void Register(MeshGeneratorInfo info);          // runtime path
		void Unregister(std::string_view name);         // script module reload

		[[nodiscard]] const MeshGeneratorInfo* Find(std::string_view name) const;
		[[nodiscard]] std::span<const MeshGeneratorInfo> All() const { return m_generators; }

	private:
		MeshGeneratorRegistry();

		template<MeshGeneratorType... Gs>
		void RegisterAll(TypeList<Gs...>) { (Register<Gs>(), ...); }

	private:
		std::vector<MeshGeneratorInfo> m_generators;
	};

	// Resolves a "proc://" URI through the registry. Null on a bad URI, unknown generator, or empty output
	[[nodiscard]] std::unique_ptr<ModelAsset> LoadProcedural(const std::string& uri);

	// Typed spelling of a spec, for C++ callers: cache.Acquire(device, ProceduralUriFor<BoxGenerator>({ .Size = s }))
	// Clamped, so an out-of-range value lands on the same key as the mesh it will actually produce
	template<MeshGeneratorType G>
	[[nodiscard]] std::string ProceduralUriFor(typename G::Params params = {})
	{
		ClampParams<G>(params);
		return ToProceduralUri(G::Name, Serializer<typename G::Params>::Serialize(params));
	}
}
