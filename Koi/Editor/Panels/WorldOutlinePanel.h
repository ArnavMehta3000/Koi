#pragma once
#include "Koi/Engine/Interop/EditorPanel.h"
#include "Koi/Engine/ECS/World.h"
#include "Koi/Editor/Selection.h"
#include <string>
#include <IconsFontAwesome6.h>

namespace koi { class AppBase; }

namespace koi::editor
{
	// The entity tree. Selecting here drives the Inspector panel; they're split so
	// each can be shown, hidden, or docked independently from the Windows menu
	class WorldOutlinePanel : public IEditorPanel
	{
	public:
		WorldOutlinePanel(AppBase& game, Selection& selection) : m_game(&game), m_selection(&selection) {}

		[[nodiscard]] std::string_view GetName() const override { return "World Outline"; }
		[[nodiscard]] const char* GetIcon() const override { return ICON_FA_SITEMAP; }
		[[nodiscard]] bool IsClosable() const override { return false; }
		void OnDraw() override;

	private:
		AppBase*   m_game      = nullptr;
		Selection* m_selection = nullptr;

		EntityID    m_renaming = 0;  // 0 = nothing being renamed
		std::string m_renameBuffer;
		bool        m_focusRename = false;
	};
}
