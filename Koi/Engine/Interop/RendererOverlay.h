#pragma once

namespace koi
{
	class CommandList;

	class IRendererOverlay
	{
	public:
		virtual ~IRendererOverlay() = default;

		virtual void OnOverlayRender(CommandList& cmd) = 0;
	};
}
