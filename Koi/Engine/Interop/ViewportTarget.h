#pragma once
#include "Koi/Engine/Graphics/Types.h"

namespace koi
{
	struct Camera;

	struct ViewportTarget
	{
		TextureHandle HDRColorBuffer;       // ScenePass writes here
		TextureHandle DepthBuffer;          // ScenePass depth (invalid if DepthBufferFormat::None)
		TextureHandle ResolvedColorBuffer;  // TonemapPass writes here; sampled by ImGui::Image
		u32 Width  = 0;
		u32 Height = 0;
	};

	// What to render this frame and from which camera. Resolved by the app host:
	// standalone -> game camera + swapchain | editor -> editor camera + viewport texture
	struct RenderView
	{
		const Camera*  Camera = nullptr;
		ViewportTarget Target;
	};
}
