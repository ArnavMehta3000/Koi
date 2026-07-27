#pragma once
#include "Koi/Engine/Core/Handle.h"
#include "Koi/Engine/Graphics/Types.h"

namespace koi
{
	class GfxDevice;
	struct ImageAsset;

	// A decoded image resident on the GPU
	struct GPUTexture
	{
		GPUTexture() = default;
		GPUTexture(const GPUTexture&)            = delete;
		GPUTexture& operator=(const GPUTexture&) = delete;
		GPUTexture(GPUTexture&&) noexcept            = default;
		GPUTexture& operator=(GPUTexture&&) noexcept = default;

		TextureHandle   Handle;  // not named Texture: that would shadow koi::Texture inside the class
		DescriptorIndex SRVIndex = INVALID_DESCRIPTOR;
		u32             Width    = 0;
		u32             Height   = 0;

		[[nodiscard]] static GPUTexture Upload(GfxDevice& device, const ImageAsset& image);

		void Release(GfxDevice& device);
	};

	using GPUTextureHandle = Handle<GPUTexture>;
}
