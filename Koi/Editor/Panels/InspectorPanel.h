#pragma once
#include "Koi/Editor/Core/Panel.h"
#include "Koi/Editor/Inspectors/MaterialInspector.h"
#include "Koi/Editor/Selection.h"
#include "Koi/Editor/Undo/UndoStack.h"
#include <IconsFontAwesome6.h>
#include <string>

namespace koi::editor
{
	class AssetDatabase;
	class EditorContext;
	class InspectorRegistry;

	class InspectorPanel : public Panel
	{
	public:
		static constexpr PanelInfo kInfo{ .Name = "Inspector", .Icon = ICON_FA_SLIDERS, .Closable = false };

		explicit InspectorPanel(EditorContext* ctx);

		void OnDraw() override;

	private:
		[[nodiscard]] UndoScope WatchComponent(std::string_view verb, std::string_view key, Entity e) const;
		void DrawAddComponentMenu(Entity e);

		std::string        m_addFilter;

		Selection*         m_selection  = nullptr;
		InspectorRegistry* m_inspectors = nullptr;
		AssetDatabase*     m_database   = nullptr;
		UndoStack*         m_undo       = nullptr;
		MaterialInspector  m_materials;
	};
}
