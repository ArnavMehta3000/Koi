#pragma once
#include "Koi/Engine/Content/Shaders/Interop/Mesh.hlsli"
#include <string>
#include <vector>

namespace koi::editor
{
	class TangentGenerator
	{
	public:
		// MikkTSpace tangents for one submesh. Indices are local to vertices
		[[nodiscard]] static bool Generate(std::vector<Vertex>& vertices, std::vector<u32>& indices);

		// Generates for a .kmesh (with w == 0), then reloads it
		static bool GenerateForAsset(const std::string& assetPath);
	};
}
