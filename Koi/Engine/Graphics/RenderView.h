#pragma once
#include "Koi/Engine/Graphics/Types.h"

namespace koi
{
	struct Camera;

	enum class ViewFlags : u8
	{
		None      = 0,
		Grid      = KOI_BIT(0),
		DebugDraw = KOI_BIT(1),  // the shared DebugDraw queue: game and editor lines
	};

	struct ViewportTarget
	{
		TextureHandle ResolvedColorBuffer;  // the pipeline's final image; sampled by ImGui::Image
		u32 Width  = 0;
		u32 Height = 0;
	};

	// What to render this frame and from which camera. Resolved by the app host:
	// standalone -> game camera + swapchain | editor -> a viewport camera + its texture
	struct RenderView
	{
		const Camera*       Camera = nullptr;
		ViewportTarget      Target;
		EnumFlag<ViewFlags> Flags;
	};
}
