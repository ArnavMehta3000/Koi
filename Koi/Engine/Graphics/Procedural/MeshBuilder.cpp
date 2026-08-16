#include "Koi/Engine/Graphics/Procedural/MeshBuilder.h"
#include <cmath>

namespace koi
{
	u32 MeshBuilder::AddVertex(const Vertex& v)
	{
		const u32 index = static_cast<u32>(m_vertices.size());
		m_vertices.push_back(v);
		return index;
	}

	u32 MeshBuilder::AddVertex(const Vector3& position, const Vector3& normal, const Vector2& uv)
	{
		return AddVertex(Vertex{ .Position = position, .Normal = normal, .Tangent = Vector4::Zero, .UV = uv });
	}

	void MeshBuilder::AddTriangle(u32 a, u32 b, u32 c)
	{
		m_indices.push_back(a);
		m_indices.push_back(b);
		m_indices.push_back(c);
	}

	void MeshBuilder::AddQuad(u32 a, u32 b, u32 c, u32 d)
	{
		AddTriangle(a, b, c);
		AddTriangle(a, c, d);
	}

	void MeshBuilder::BeginSubMesh(i32 materialIndex)
	{
		CloseSubMesh();
		m_subMeshes.push_back(SubMesh
		{
			.IndexOffset   = static_cast<u32>(m_indices.size()),
			.MaterialIndex = materialIndex,
		});
	}

	void MeshBuilder::CloseSubMesh()
	{
		if (m_subMeshes.empty()) return;

		SubMesh& open = m_subMeshes.back();
		open.IndexCount = static_cast<u32>(m_indices.size()) - open.IndexOffset;

		if (open.IndexCount == 0)
			m_subMeshes.pop_back();
	}

	// Cross products are left unnormalized so each face contributes in proportion to its area
	void MeshBuilder::ComputeNormals()
	{
		for (Vertex& v : m_vertices)
		{
			v.Normal = Vector3::Zero;
		}

		for (size_t i = 0; i + 2 < m_indices.size(); i += 3)
		{
			Vertex& a = m_vertices[m_indices[i]];
			Vertex& b = m_vertices[m_indices[i + 1]];
			Vertex& c = m_vertices[m_indices[i + 2]];

			const Vector3 face = (b.Position - a.Position).Cross(c.Position - a.Position);
			a.Normal += face;
			b.Normal += face;
			c.Normal += face;
		}

		for (Vertex& v : m_vertices)
		{
			if (v.Normal.LengthSquared() > 0.0f)
				v.Normal.Normalize();
			else
				v.Normal = Vector3::Up;
		}
	}

	// Lengyel's method: accumulate per-triangle UV gradients, then orthonormalize against the normal
	void MeshBuilder::ComputeTangents()
	{
		std::vector<Vector3> tangents(m_vertices.size(), Vector3::Zero);
		std::vector<Vector3> bitangents(m_vertices.size(), Vector3::Zero);

		for (size_t i = 0; i + 2 < m_indices.size(); i += 3)
		{
			const u32 ia = m_indices[i];
			const u32 ib = m_indices[i + 1];
			const u32 ic = m_indices[i + 2];

			const Vertex& a = m_vertices[ia];
			const Vertex& b = m_vertices[ib];
			const Vertex& c = m_vertices[ic];

			const Vector3 e1 = b.Position - a.Position;
			const Vector3 e2 = c.Position - a.Position;
			const Vector2 d1 = b.UV - a.UV;
			const Vector2 d2 = c.UV - a.UV;

			const f32 det = d1.x * d2.y - d2.x * d1.y;
			if (std::abs(det) < 1e-12f) continue;  // degenerate UVs contribute nothing

			const f32 r = 1.0f / det;
			const Vector3 tangent   = (e1 * d2.y - e2 * d1.y) * r;
			const Vector3 bitangent = (e2 * d1.x - e1 * d2.x) * r;

			for (const u32 index : { ia, ib, ic })
			{
				tangents[index]   += tangent;
				bitangents[index] += bitangent;
			}
		}

		for (size_t i = 0; i < m_vertices.size(); ++i)
		{
			Vertex& v = m_vertices[i];
			const Vector3& n = v.Normal;

			// Gram-Schmidt; fall back to any axis perpendicular to the normal when UVs gave us nothing
			Vector3 t = tangents[i] - n * n.Dot(tangents[i]);
			if (t.LengthSquared() < 1e-16f)
			{
				t = std::abs(n.x) < 0.9f ? Vector3::Right.Cross(n) : Vector3::Up.Cross(n);
			}

			t.Normalize();

			// w reconstructs the bitangent in the shader as cross(N, T.xyz) * T.w
			const f32 handedness = n.Cross(t).Dot(bitangents[i]) < 0.0f ? -1.0f : 1.0f;
			v.Tangent = Vector4(t.x, t.y, t.z, handedness);
		}
	}

	void MeshBuilder::Transform(const Matrix& m)
	{
		const Matrix normalMatrix = m.Invert().Transpose();
		const bool   mirrored     = m.Determinant() < 0.0f;

		for (Vertex& v : m_vertices)
		{
			v.Position = Vector3::Transform(v.Position, m);

			v.Normal = Vector3::TransformNormal(v.Normal, normalMatrix);
			v.Normal.Normalize();

			Vector3 tangent = Vector3::TransformNormal(Vector3(v.Tangent.x, v.Tangent.y, v.Tangent.z), m);
			tangent.Normalize();

			const f32 handedness = mirrored ? -v.Tangent.w : v.Tangent.w;
			v.Tangent = Vector4(tangent.x, tangent.y, tangent.z, handedness);
		}
	}

	ModelAsset MeshBuilder::Build(std::string name) &&
	{
		CloseSubMesh();

		// Generators that never called BeginSubMesh get one covering everything
		if (m_subMeshes.empty() && !m_indices.empty())
		{
			m_subMeshes.push_back(SubMesh{ .IndexOffset = 0, .IndexCount = static_cast<u32>(m_indices.size()) });
		}

		Mesh mesh;
		mesh.Name = name;

		// Indices are absolute into the single vertex array, so VertexOffset stays 0 and
		// GPUMesh::Upload's baseVertex is the only rebasing that happens
		std::vector<Vector3> positions;
		for (SubMesh& sub : m_subMeshes)
		{
			positions.clear();
			positions.reserve(sub.IndexCount);
			for (u32 i = 0; i < sub.IndexCount; ++i)
			{
				positions.push_back(m_vertices[m_indices[sub.IndexOffset + i]].Position);
			}

			if (!positions.empty())
			{
				BoundingBox::CreateFromPoints(sub.Bounds, positions.size(),
					reinterpret_cast<const DirectX::XMFLOAT3*>(positions.data()), sizeof(Vector3));
			}

			MergeBounds(mesh.Bounds, sub.Bounds);
		}

		mesh.SubMeshes   = std::move(m_subMeshes);
		mesh.VertexCount = static_cast<u32>(m_vertices.size());
		mesh.IndexCount  = static_cast<u32>(m_indices.size());
		mesh.Vertices    = std::move(m_vertices);
		mesh.Indices     = std::move(m_indices);

		ModelAsset model;
		model.SourcPath = name;
		model.Bounds    = mesh.Bounds;
		model.Meshes.push_back(std::move(mesh));
		return model;
	}
}
