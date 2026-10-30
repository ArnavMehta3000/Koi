#pragma once
#include "Koi/Editor/Core/Panel.h"
#include "Koi/Editor/Selection.h"
#include "Koi/Engine/ECS/World.h"
#include <string>
#include <vector>
#include <IconsFontAwesome6.h>

namespace koi { class AppBase; }

namespace koi::editor
{
	class EditorContext;

	// The entity tree. Selecting here drives the Inspector panel; they're split so
	// each can be shown, hidden, or docked independently from the Windows menu
	class WorldOutlinePanel : public Panel
	{
	public:
		static constexpr PanelInfo kInfo{ .Name = "World Outline", .Icon = ICON_FA_SITEMAP, .Closable = false };

		explicit WorldOutlinePanel(EditorContext* ctx);

		void OnDraw() override;

	private:
		AppBase*   m_game      = nullptr;
		Selection* m_selection = nullptr;

		EntityID    m_renaming = 0;  // 0 = nothing being renamed
		std::string m_renameBuffer;
		bool        m_focusRename = false;

		std::vector<Entity> m_order;  // per-draw scratch, kept for its capacity
	};
}
