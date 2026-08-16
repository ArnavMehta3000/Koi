#pragma once
#include "Koi/Engine/Graphics/Types.h"
#include <concepts>
#include <ranges>
#include <utility>

namespace koi
{
	// Anything convertible to Vertex, so generators can push whatever aggregate suits them
	template<typename T>
	concept VertexLike = std::convertible_to<T, Vertex>;

	// Front faces wind counter-clockwise viewed from outside (RH), matching the
	// FrontCounterClockwise raster state every mesh pipeline uses
	class MeshBuilder
	{
	public:
		template<typename Self>
		auto&& Reserve(this Self&& self, u32 vertexCount, u32 indexCount)
		{
			self.m_vertices.reserve(self.m_vertices.size() + vertexCount);
			self.m_indices.reserve(self.m_indices.size() + indexCount);
			return std::forward<Self>(self);
		}

		u32 AddVertex(const Vertex& v);
		u32 AddVertex(const Vector3& position, const Vector3& normal, const Vector2& uv);

		template<std::ranges::input_range R> requires VertexLike<std::ranges::range_value_t<R>>
		u32 AddVertices(R&& range)          // returns the first new index
		{
			const u32 first = static_cast<u32>(m_vertices.size());

			if constexpr (std::ranges::sized_range<R>)
				m_vertices.reserve(m_vertices.size() + std::ranges::size(range));

			for (auto&& vertex : range)
			{
				m_vertices.emplace_back(static_cast<Vertex>(std::forward<decltype(vertex)>(vertex)));
			}

			return first;
		}

		void AddTriangle(u32 a, u32 b, u32 c);
		void AddQuad(u32 a, u32 b, u32 c, u32 d);

		// Fan triangulation over a pack; the ring/cap loops in cylinder and cone want this
		template<std::same_as<u32>... Is> requires (sizeof...(Is) >= 3)
		void AddPolygon(Is... indices)
		{
			const std::array<u32, sizeof...(Is)> ring{ indices... };
			for (size_t i = 1; i + 1 < ring.size(); ++i)
			{
				AddTriangle(ring[0], ring[i], ring[i + 1]);
			}
		}

		void BeginSubMesh(i32 materialIndex = -1);   // closes the previous one; optional

		void ComputeNormals();     // area-weighted accumulate, then normalize
		void ComputeTangents();    // from UVs; writes Vertex::Tangent.w handedness
		void Transform(const Matrix& m);

		[[nodiscard]] bool IsEmpty() const { return m_indices.empty(); }

		[[nodiscard]] ModelAsset Build(std::string name) &&;   // fills SubMesh/Mesh/Model bounds

	private:
		// Terminates the open submesh, discarding it if it covers no indices
		void CloseSubMesh();

	private:
		std::vector<Vertex>  m_vertices;
		std::vector<u32>     m_indices;
		std::vector<SubMesh> m_subMeshes;
	};
}
