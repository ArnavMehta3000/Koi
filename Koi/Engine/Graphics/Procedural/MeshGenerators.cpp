#include "Koi/Engine/Graphics/Procedural/MeshGenerators.h"
#include "Koi/Engine/Graphics/Procedural/ProceduralMeshSpec.h"
#include "Koi/Engine/Logging/Logger.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace koi
{
	namespace
	{
		constexpr f32 kTwoPi = 2.0f * std::numbers::pi_v<f32>;
		constexpr f32 kPi    = std::numbers::pi_v<f32>;

		// "no vertex recorded yet", so the ring loops can capture their own base index
		constexpr u32 kNoIndex = ~0u;

		[[nodiscard]] f32 Frac(u32 i, u32 count) { return static_cast<f32>(i) / static_cast<f32>(count); }

		// Stitches a (rows+1) x (cols+1) row-major vertex grid starting at `base`.
		// Rows must advance in the direction that leaves faces wound CCW from outside
		void StitchGrid(MeshBuilder& builder, u32 base, u32 rows, u32 cols,
		                bool poleFirstRow = false, bool poleLastRow = false)
		{
			const u32 stride = cols + 1;

			for (u32 r = 0; r < rows; ++r)
			{
				for (u32 c = 0; c < cols; ++c)
				{
					const u32 v00 = base + r * stride + c;

					// A pole row collapses to one point, so one of the two triangles is degenerate
					if (poleFirstRow && r == 0)
						builder.AddTriangle(v00, v00 + 1 + stride, v00 + stride);
					else if (poleLastRow && r == rows - 1)
						builder.AddTriangle(v00, v00 + 1, v00 + 1 + stride);
					else
						builder.AddQuad(v00, v00 + 1, v00 + 1 + stride, v00 + stride);
				}
			}
		}

		// One planar grid centred on normal * offset, where right x up == normal
		void AddFace(MeshBuilder& builder, const Vector3& right, const Vector3& up, const Vector3& normal,
		             f32 halfRight, f32 halfUp, f32 offset, u32 segsRight, u32 segsUp)
		{
			const Vector3 origin = normal * offset;
			u32           first  = kNoIndex;

			for (u32 j = 0; j <= segsUp; ++j)
			{
				const f32 v = Frac(j, segsUp);
				for (u32 i = 0; i <= segsRight; ++i)
				{
					const f32 u = Frac(i, segsRight);
					const Vector3 position = origin
						+ right * ((u * 2.0f - 1.0f) * halfRight)
						+ up    * ((v * 2.0f - 1.0f) * halfUp);

					const u32 index = builder.AddVertex(position, normal, Vector2(u, 1.0f - v));
					if (first == kNoIndex) first = index;
				}
			}

			StitchGrid(builder, first, segsUp, segsRight);
		}

		// A closed fan around `center`, over `count` ring vertices starting at `ringBase`.
		// `flip` reverses the winding for a downward-facing cap
		void AddFan(MeshBuilder& builder, u32 center, u32 ringBase, u32 count, bool flip)
		{
			for (u32 i = 0; i < count; ++i)
			{
				const u32 a = ringBase + i;
				const u32 b = ringBase + i + 1;

				if (flip)
					builder.AddTriangle(center, a, b);
				else
					builder.AddTriangle(center, b, a);
			}
		}

		// Generate takes params that Register<G> has already run through Clamp, so these only
		// widen the type. The limits themselves live in the Clamp bodies below, which the
		// inspector also drives its widgets from
		[[nodiscard]] u32 Segments(i32 value) { return static_cast<u32>(value); }

		constexpr f32 kMinExtent = 1e-3f;   // below this a shape is invisible but still uploads
		constexpr i32 kMaxDetail = 512;     // guards against an accidental million-triangle sphere

		void ClampExtent(f32& value) { value = std::clamp(value, kMinExtent, 1e6f); }
		void ClampDetail(i32& value, i32 minimum) { value = std::clamp(value, minimum, kMaxDetail); }
	}

	void PlaneGenerator::Clamp(Params& p)
	{
		ClampExtent(p.Size.x);
		ClampExtent(p.Size.y);
		ClampDetail(p.SegmentsX, 1);
		ClampDetail(p.SegmentsZ, 1);
	}

	void BoxGenerator::Clamp(Params& p)
	{
		ClampExtent(p.Size.x);
		ClampExtent(p.Size.y);
		ClampExtent(p.Size.z);
		ClampDetail(p.Segments, 1);
	}

	void SphereGenerator::Clamp(Params& p)
	{
		ClampExtent(p.Radius);
		ClampDetail(p.Segments, 3);
		ClampDetail(p.Rings, 2);
	}

	void CylinderGenerator::Clamp(Params& p)
	{
		ClampExtent(p.Radius);
		ClampExtent(p.Height);
		ClampDetail(p.Segments, 3);
	}

	void ConeGenerator::Clamp(Params& p)
	{
		ClampExtent(p.Radius);
		ClampExtent(p.Height);
		ClampDetail(p.Segments, 3);
	}

	void CapsuleGenerator::Clamp(Params& p)
	{
		ClampExtent(p.Radius);
		p.Height = std::clamp(p.Height, 0.0f, 1e6f);   // zero is legal: the capsule becomes a sphere
		ClampDetail(p.Segments, 3);
		ClampDetail(p.Rings, 1);
	}

	void TorusGenerator::Clamp(Params& p)
	{
		ClampExtent(p.Radius);
		ClampExtent(p.TubeRadius);   // may exceed Radius: a self-intersecting torus is still a shape
		ClampDetail(p.Segments, 3);
		ClampDetail(p.Sides, 3);
	}

	void PlaneGenerator::Generate(MeshBuilder& builder, const Params& p)
	{
		const u32 segsX = Segments(p.SegmentsX);
		const u32 segsZ = Segments(p.SegmentsZ);

		// right x up == +Y, so the grid faces up and the quad order stays CCW from above
		AddFace(builder, Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, -1.0f), Vector3::Up,
		        p.Size.x * 0.5f, p.Size.y * 0.5f, 0.0f, segsX, segsZ);

		builder.ComputeTangents();
	}

	void BoxGenerator::Generate(MeshBuilder& builder, const Params& p)
	{
		const u32 segs = Segments(p.Segments);
		const f32 hx = p.Size.x * 0.5f;
		const f32 hy = p.Size.y * 0.5f;
		const f32 hz = p.Size.z * 0.5f;

		builder.Reserve(6 * (segs + 1) * (segs + 1), 6 * segs * segs * 6);

		// Each face gets its own vertices so normals stay flat and UVs restart per face
		AddFace(builder, Vector3(0, 0, -1), Vector3(0,  1,  0), Vector3( 1,  0,  0), hz, hy, hx, segs, segs);
		AddFace(builder, Vector3(0, 0,  1), Vector3(0,  1,  0), Vector3(-1,  0,  0), hz, hy, hx, segs, segs);
		AddFace(builder, Vector3(1, 0,  0), Vector3(0,  0, -1), Vector3( 0,  1,  0), hx, hz, hy, segs, segs);
		AddFace(builder, Vector3(1, 0,  0), Vector3(0,  0,  1), Vector3( 0, -1,  0), hx, hz, hy, segs, segs);
		AddFace(builder, Vector3(1, 0,  0), Vector3(0,  1,  0), Vector3( 0,  0,  1), hx, hy, hz, segs, segs);
		AddFace(builder, Vector3(-1, 0, 0), Vector3(0,  1,  0), Vector3( 0,  0, -1), hx, hy, hz, segs, segs);

		builder.ComputeTangents();
	}

	void SphereGenerator::Generate(MeshBuilder& builder, const Params& p)
	{
		const u32 segs  = Segments(p.Segments);
		const u32 rings = Segments(p.Rings);
		const f32 radius = p.Radius;

		builder.Reserve((rings + 1) * (segs + 1), rings * segs * 6);

		// Rows run +Y pole to -Y pole; the seam column is duplicated so UVs wrap cleanly
		u32 first = kNoIndex;
		for (u32 ring = 0; ring <= rings; ++ring)
		{
			const f32 theta = kPi * Frac(ring, rings);
			const f32 sinTheta = std::sin(theta);
			const f32 cosTheta = std::cos(theta);

			for (u32 seg = 0; seg <= segs; ++seg)
			{
				const f32 phi = kTwoPi * Frac(seg, segs);
				const Vector3 normal(sinTheta * std::cos(phi), cosTheta, sinTheta * std::sin(phi));

				const u32 index = builder.AddVertex(normal * radius, normal,
					Vector2(Frac(seg, segs), Frac(ring, rings)));

				if (first == kNoIndex) first = index;
			}
		}

		StitchGrid(builder, first, rings, segs, true, true);
		builder.ComputeTangents();
	}

	void CylinderGenerator::Generate(MeshBuilder& builder, const Params& p)
	{
		const u32 segs   = Segments(p.Segments);
		const f32 radius = p.Radius;
		const f32 halfH  = p.Height * 0.5f;

		// Side: row 0 is the top ring, row 1 the bottom, so the quads wind outward
		u32 first = kNoIndex;
		for (u32 row = 0; row <= 1; ++row)
		{
			const f32 y = row == 0 ? halfH : -halfH;

			for (u32 seg = 0; seg <= segs; ++seg)
			{
				const f32 phi = kTwoPi * Frac(seg, segs);
				const Vector3 normal(std::cos(phi), 0.0f, std::sin(phi));

				const u32 index = builder.AddVertex(Vector3(normal.x * radius, y, normal.z * radius),
					normal, Vector2(Frac(seg, segs), static_cast<f32>(row)));

				if (first == kNoIndex) first = index;
			}
		}

		StitchGrid(builder, first, 1, segs);

		if (p.Caps)
		{
			// Caps need their own vertices: same positions, axial normals
			for (const f32 y : { halfH, -halfH })
			{
				const Vector3 normal = y > 0.0f ? Vector3::Up : Vector3::Down;
				const u32 center = builder.AddVertex(Vector3(0.0f, y, 0.0f), normal, Vector2(0.5f, 0.5f));

				u32 ringBase = kNoIndex;
				for (u32 seg = 0; seg <= segs; ++seg)
				{
					const f32 phi = kTwoPi * Frac(seg, segs);
					const f32 c = std::cos(phi);
					const f32 s = std::sin(phi);

					const u32 index = builder.AddVertex(Vector3(c * radius, y, s * radius), normal,
						Vector2(0.5f + 0.5f * c, 0.5f + 0.5f * s));

					if (ringBase == kNoIndex) ringBase = index;
				}

				AddFan(builder, center, ringBase, segs, y < 0.0f);
			}
		}

		builder.ComputeTangents();
	}

	void ConeGenerator::Generate(MeshBuilder& builder, const Params& p)
	{
		const u32 segs   = Segments(p.Segments);
		const f32 radius = p.Radius;
		const f32 height = p.Height;
		const f32 halfH  = height * 0.5f;

		// The slant normal leans by radius/height; the apex is split per segment so each
		// side triangle gets the normal of its own slice rather than an averaged spike
		auto slantNormal = [&](f32 phi)
		{
			Vector3 n(std::cos(phi) * height, radius, std::sin(phi) * height);
			n.Normalize();
			return n;
		};

		for (u32 seg = 0; seg < segs; ++seg)
		{
			const f32 phi0 = kTwoPi * Frac(seg, segs);
			const f32 phi1 = kTwoPi * Frac(seg + 1, segs);
			const f32 phiMid = (phi0 + phi1) * 0.5f;

			const u32 a = builder.AddVertex(Vector3(std::cos(phi0) * radius, -halfH, std::sin(phi0) * radius),
				slantNormal(phi0), Vector2(Frac(seg, segs), 0.0f));

			const u32 apex = builder.AddVertex(Vector3(0.0f, halfH, 0.0f),
				slantNormal(phiMid), Vector2(Frac(seg, segs) + 0.5f / static_cast<f32>(segs), 1.0f));

			const u32 b = builder.AddVertex(Vector3(std::cos(phi1) * radius, -halfH, std::sin(phi1) * radius),
				slantNormal(phi1), Vector2(Frac(seg + 1, segs), 0.0f));

			builder.AddTriangle(a, apex, b);
		}

		if (p.Cap)
		{
			const u32 center = builder.AddVertex(Vector3(0.0f, -halfH, 0.0f), Vector3::Down, Vector2(0.5f, 0.5f));

			u32 ringBase = kNoIndex;
			for (u32 seg = 0; seg <= segs; ++seg)
			{
				const f32 phi = kTwoPi * Frac(seg, segs);
				const f32 c = std::cos(phi);
				const f32 s = std::sin(phi);

				const u32 index = builder.AddVertex(Vector3(c * radius, -halfH, s * radius), Vector3::Down,
					Vector2(0.5f + 0.5f * c, 0.5f + 0.5f * s));

				if (ringBase == kNoIndex) ringBase = index;
			}

			AddFan(builder, center, ringBase, segs, true);
		}

		builder.ComputeTangents();
	}

	void CapsuleGenerator::Generate(MeshBuilder& builder, const Params& p)
	{
		const u32 segs   = Segments(p.Segments);
		const u32 rings  = Segments(p.Rings);
		const f32 radius = p.Radius;
		const f32 halfH  = p.Height * 0.5f;

		// One lat-long sweep with the equator split in two: rows [0, rings] are the top cap
		// offset to +halfH, rows [rings+1, 2*rings+1] the bottom at -halfH. The band between
		// the two equator rows is the cylindrical section, so no separate side pass is needed
		const u32 totalRows = 2 * rings + 1;

		u32 first = kNoIndex;
		for (u32 row = 0; row <= totalRows; ++row)
		{
			const bool bottom = row > rings;
			const u32  local  = bottom ? row - (rings + 1) : row;

			const f32 theta = bottom
				? kPi * 0.5f + kPi * 0.5f * Frac(local, rings)
				: kPi * 0.5f * Frac(local, rings);

			const f32 yOffset  = bottom ? -halfH : halfH;
			const f32 sinTheta = std::sin(theta);
			const f32 cosTheta = std::cos(theta);

			for (u32 seg = 0; seg <= segs; ++seg)
			{
				const f32 phi = kTwoPi * Frac(seg, segs);
				const Vector3 normal(sinTheta * std::cos(phi), cosTheta, sinTheta * std::sin(phi));
				const Vector3 position = normal * radius + Vector3(0.0f, yOffset, 0.0f);

				const u32 index = builder.AddVertex(position, normal,
					Vector2(Frac(seg, segs), Frac(row, totalRows)));

				if (first == kNoIndex) first = index;
			}
		}

		StitchGrid(builder, first, totalRows, segs, true, true);
		builder.ComputeTangents();
	}

	void TorusGenerator::Generate(MeshBuilder& builder, const Params& p)
	{
		const u32 segs  = Segments(p.Segments);
		const u32 sides = Segments(p.Sides);
		const f32 major = p.Radius;
		const f32 minor = p.TubeRadius;

		builder.Reserve((segs + 1) * (sides + 1), segs * sides * 6);

		// Rows sweep the major radius, columns the tube. Both seams are duplicated for UVs
		u32 first = kNoIndex;
		for (u32 seg = 0; seg <= segs; ++seg)
		{
			const f32 u = kTwoPi * Frac(seg, segs);
			const f32 cosU = std::cos(u);
			const f32 sinU = std::sin(u);

			for (u32 side = 0; side <= sides; ++side)
			{
				const f32 v = kTwoPi * Frac(side, sides);
				const f32 cosV = std::cos(v);
				const f32 sinV = std::sin(v);

				const Vector3 normal(cosV * cosU, sinV, cosV * sinU);
				const Vector3 position((major + minor * cosV) * cosU, minor * sinV, (major + minor * cosV) * sinU);

				const u32 index = builder.AddVertex(position, normal,
					Vector2(Frac(seg, segs), Frac(side, sides)));

				if (first == kNoIndex) first = index;
			}
		}

		StitchGrid(builder, first, segs, sides);
		builder.ComputeTangents();
	}

	// --- Registry ---

	MeshGeneratorRegistry& MeshGeneratorRegistry::Get()
	{
		static MeshGeneratorRegistry s_instance;
		return s_instance;
	}

	MeshGeneratorRegistry::MeshGeneratorRegistry()
	{
		RegisterAll(Primitives{});
		KOI_LOG_DEBUG("Mesh generator registry: {} generator(s) registered", m_generators.size());
	}

	void MeshGeneratorRegistry::Register(MeshGeneratorInfo info)
	{
		// Re-registering a name replaces it, so a reloaded script module doesn't need to Unregister first
		const auto existing = std::ranges::find(m_generators, info.Name, &MeshGeneratorInfo::Name);
		if (existing != m_generators.end())
		{
			*existing = std::move(info);
			return;
		}

		m_generators.push_back(std::move(info));
	}

	void MeshGeneratorRegistry::Unregister(std::string_view name)
	{
		std::erase_if(m_generators, [name](const MeshGeneratorInfo& info) { return info.Name == name; });
	}

	const MeshGeneratorInfo* MeshGeneratorRegistry::Find(std::string_view name) const
	{
		const auto it = std::ranges::find(m_generators, name, &MeshGeneratorInfo::Name);
		return it != m_generators.end() ? &*it : nullptr;
	}

	std::unique_ptr<ModelAsset> LoadProcedural(const std::string& uri)
	{
		const std::optional parsed = ParseProceduralUri(uri);
		if (!parsed)
		{
			KOI_LOG_ERROR("Malformed procedural mesh URI: {}", uri);
			return nullptr;
		}

		const auto& [generator, params] = *parsed;

		const MeshGeneratorInfo* info = MeshGeneratorRegistry::Get().Find(generator);
		if (!info)
		{
			KOI_LOG_ERROR("Unknown procedural mesh generator '{}' in: {}", generator, uri);
			return nullptr;
		}

		std::expected<ModelAsset, std::string> model = info->Generate(params);
		if (!model)
		{
			KOI_LOG_ERROR("Procedural mesh '{}' failed: {}", uri, model.error());
			return nullptr;
		}

		model->SourcPath = uri;
		return std::make_unique<ModelAsset>(std::move(*model));
	}
}
