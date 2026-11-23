#pragma once
#include "Koi/Engine/Assets/AssetKey.h"
#include "Koi/Engine/Content/Shaders/Interop/Mesh.hlsli"
#include "Koi/Engine/RHI/Types.h"
#include "Koi/Engine/Math/MathTypes.h"
#include <memory>
#include <string>
#include <vector>

// CPU-side data an asset decodes into before it is uploaded
namespace koi
{
	struct PreparedTexture;
	struct PreparedMesh;

	struct ImageAsset
	{
		std::string Name;
		std::string Path;
		u32 MaterialIndex = 0;
		i32 Width         = -1;
		i32 Height        = -1;
		i32 Channels      = -1;
		std::vector<koi::byte> Pixels;

		// Set for a .ktex: every mip, already compressed and laid out for upload. Pixels stays empty
		std::shared_ptr<const PreparedTexture> Prepared;
	};

	struct ImportMaterial
	{
		enum class AlphaMode { Opaque, Mask, Blend };
		struct Constants
		{
			Vector4 BaseColorFactor   = Vector4(1.0f, 1.0f, 1.0, 1.0f);
			f32     MetallicFactor    = 1.0f;
			f32     RoughnessFactor   = 1.0f;
			f32     NormalScale       = 1.0f;
			f32     OcclusionStrength = 1.0f;
			Vector3 EmissiveFactor    = Vector3(0.0f, 0.0f, 0.0f);
			f32     AlphaCutoff       = 0.5f;
		};

		std::string Name;
		Constants MaterialConstants{};
		bool DoubleSided = false;
		AlphaMode Mode;

		DescriptorIndex BaseColorTextureIdx         = INVALID_DESCRIPTOR;
		DescriptorIndex MetallicRoughnessTextureIdx = INVALID_DESCRIPTOR;
		DescriptorIndex NormalTextureIdx            = INVALID_DESCRIPTOR;
		DescriptorIndex OcclusionTextureIdx         = INVALID_DESCRIPTOR;
		DescriptorIndex EmissiveTextureIdx          = INVALID_DESCRIPTOR;
	};

	struct SubMesh
	{
		u32 IndexOffset   = 0;
		u32 IndexCount    = 0;
		u32 VertexOffset  = 0;
		i32 MaterialIndex = -1;   // index into Scene::Materials, -1 = default
		BoundingBox Bounds{ {}, {} };
	};

	struct Mesh
	{
		std::string Name;
		std::vector<SubMesh> SubMeshes;
		std::vector<Vertex> Vertices;
		std::vector<u32> Indices;
		BoundingBox Bounds{ {}, {} };
		u32 VertexCount = 0;
		u32 IndexCount = 0;
	};

	struct ModelAsset
	{
		std::string SourcePath;
		std::vector<Mesh> Meshes;
		std::vector<ImportMaterial> Materials;
		std::vector<AssetKey> MaterialBindings;  // slot -> material asset, from the mesh sidecar
		std::vector<ImageAsset> Images;
		BoundingBox Bounds{ {}, {} };

		// Set for a .kmesh: geometry straight from the file, Meshes and Materials stay empty
		std::shared_ptr<const PreparedMesh> Prepared;
	};
}
