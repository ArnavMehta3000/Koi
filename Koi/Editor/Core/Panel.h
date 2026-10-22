#pragma once
#include <imgui.h>
#include <toml++/toml.hpp>
#include <string>
#include <string_view>

namespace koi::editor
{
	class PanelManager;

	// What a panel type is, as a `static constexpr PanelInfo kInfo` on the type
	struct PanelInfo
	{
		std::string_view Name;                // window title, ID and Editor.toml section, so it must not change
		const char*      Icon        = nullptr;  // Font Awesome glyph before the name
		ImGuiWindowFlags WindowFlags = 0;
		bool             Padding     = true;
		bool             Closable    = true;
	};

	// The editor owns the panel's window; OnDraw only fills it
	class Panel
	{
	public:
		virtual ~Panel() = default;

		virtual void OnDraw() = 0;

		// Every frame while open, just before the window begins (even when docked out of sight)
		virtual void OnUpdate() {}

		// Per-project state in <project>/.koi/Editor.toml. LoadState always runs once, with an empty table if nothing was saved
		virtual void SaveState([[maybe_unused]] toml::table& state) const {}
		virtual void LoadState([[maybe_unused]] const toml::table& state) {}

		[[nodiscard]] const PanelInfo& GetInfo() const { return *m_info; }
		[[nodiscard]] bool IsOpen() const { return m_open; }
		void SetOpen(bool open) { m_open = open; }

		// Opens the panel and brings it to the front on the next frame
		void Focus() { m_open = true; m_focusPending = true; }

	private:
		friend class PanelManager;

		const PanelInfo* m_info = nullptr;
		bool             m_open = true;
		bool             m_focusPending = false;
	};
}
