#pragma once
#include "Koi/Editor/Core/Panel.h"
#include "Koi/Editor/Inspectors/MaterialInspector.h"
#include "Koi/Editor/Selection.h"
#include <IconsFontAwesome6.h>

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
		Selection*         m_selection  = nullptr;
		InspectorRegistry* m_inspectors = nullptr;
		AssetDatabase*     m_database   = nullptr;
		MaterialInspector  m_materials;
	};
}
