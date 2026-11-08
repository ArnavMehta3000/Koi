#pragma once
#include "Koi/Engine/Graphics/Types.h"

namespace koi
{
	struct Camera;

	enum class ViewFlags : u8
	{
		None = 0,
		Grid = KOI_BIT(0),
	};

	struct ViewportTarget
	{
		TextureHandle ResolvedColorBuffer;  // the pipeline's final image; sampled by ImGui::Image
		u32 Width  = 0;
		u32 Height = 0;
	};

	// What to render this frame and from which camera. Resolved by the app host:
	// standalone -> game camera + swapchain | editor -> editor camera + viewport texture
	struct RenderView
	{
		const Camera*       Camera = nullptr;
		ViewportTarget      Target;
		EnumFlag<ViewFlags> Flags;
	};
}
