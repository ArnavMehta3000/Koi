#pragma once
#include "Koi/Engine/Graphics/Types.h"

namespace koi
{
	struct ViewportTarget
	{
		TextureHandle HDRColorBuffer;       // ScenePass writes here
		TextureHandle DepthBuffer;          // ScenePass depth (invalid if DepthBufferFormat::None)
		TextureHandle ResolvedColorBuffer;  // TonemapPass writes here; sampled by ImGui::Image
		u32 Width  = 0;
		u32 Height = 0;
	};
}
