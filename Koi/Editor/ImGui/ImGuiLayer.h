#pragma once
#include "Koi/Engine/Core/Types.h"

namespace koi
{
	class CommandList;
	class Window;
}

namespace koi::editor
{
	enum class Theme : u8;

	class ImGuiLayer : public NonCopyable, public NonMovable
	{
	public:
		~ImGuiLayer();

		void Init(Window& window);
		void Shutdown();

		void BeginFrame();
		void EndFrame(CommandList& cmd);

		// Contributed to whichever menu the editor puts it in
		void DrawThemeMenu();

	private:
		Theme m_theme{};  // real default is applied in Init, where Themes.h is visible
		bool  m_initialized = false;
	};
}
