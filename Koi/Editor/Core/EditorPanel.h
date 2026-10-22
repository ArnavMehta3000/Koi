#pragma once
#include <imgui.h>
#include <string_view>

namespace koi::editor
{
	// The editor owns the panel's window; OnDraw only fills it
	class IEditorPanel
	{
	public:
		virtual ~IEditorPanel() = default;

		// Window title and ID, so it must be a literal
		[[nodiscard]] virtual std::string_view GetName() const = 0;
		virtual void OnDraw() = 0;

		// Font Awesome glyph shown before the name in the title and the View menu
		[[nodiscard]] virtual const char* GetIcon() const { return nullptr; }

		// Every frame while open, just before the window begins (even when docked out of sight)
		virtual void OnUpdate() {}

		[[nodiscard]] virtual ImGuiWindowFlags GetWindowFlags() const { return 0; }
		[[nodiscard]] virtual bool HasWindowPadding() const { return true; }

		// Whether this panel can be toggled off
		[[nodiscard]] virtual bool IsClosable() const { return true; }

		[[nodiscard]] bool IsOpen() const { return m_open; }
		void SetOpen(bool open) { m_open = open; }

	private:
		bool m_open = true;
	};
}
