#pragma once
#include "Koi/Engine/RHI/RenderGraph/RGHandles.h"
#include "Koi/Engine/Math/MathTypes.h"

namespace koi
{
	// Blackboard entries passes publish for each other

	struct ShadowData  // absent when nothing casts a shadow this frame
	{
		RGTexture ShadowMap;
		Matrix    LightViewProjection = Matrix::Identity;
	};

	struct GBufferData
	{
		RGTexture Albedo;    // RGB + AO (RGBA8_UNORM_SRGB: encoded on write, decoded on read)
		RGTexture Normals;   // XYZ + MatID (RGBA16_UNORM)
		RGTexture Material;  // Roughness/Metallic/Specular + unused A (RGBA8_UNORM)
	};

	struct SceneDepth
	{
		RGTexture Depth;  // D32
	};

	struct SceneColor  // lit HDR
	{
		RGTexture Color;
	};

	struct ExposureData
	{
		RGBuffer Exposure;  // one float the tonemapper scales SceneColor by
	};

	struct DisplayColor  // tonemapped LDR; overlays draw on it
	{
		RGTexture Color;
	};
}
