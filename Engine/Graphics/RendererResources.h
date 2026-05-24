#pragma once
#include "Graphics/Types.h"

namespace koi
{
	class GfxDevice;

	struct RendererResources
	{
		TextureHandle   WhiteTexture{};       // 1x1 opaque white (0xFFFFFFFF)
		TextureHandle   FlatNormalTexture{};  // 1x1 flat normal (128, 128, 255, 255)
		DescriptorIndex LinearWrapSampler  = INVALID_DESCRIPTOR;
		DescriptorIndex LinearClampSampler = INVALID_DESCRIPTOR;
		DescriptorIndex PointClampSampler  = INVALID_DESCRIPTOR;

		void Init(GfxDevice& device);
		void Shutdown(GfxDevice& device);
	};
}
