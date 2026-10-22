#pragma once
#include "Koi/Editor/Core/EditorPanel.h"
#include "Koi/Editor/Inspectors/MaterialInspector.h"
#include "Koi/Editor/Selection.h"
#include <IconsFontAwesome6.h>

namespace koi::editor
{
	class AssetDatabase;
	class EditorContext;
	class InspectorRegistry;

	class InspectorPanel : public IEditorPanel
	{
	public:
		explicit InspectorPanel(EditorContext* ctx);

		[[nodiscard]] std::string_view GetName() const override { return "Inspector"; }
		[[nodiscard]] const char* GetIcon() const override { return ICON_FA_SLIDERS; }
		[[nodiscard]] bool IsClosable() const override { return false; }
		void OnDraw() override;

	private:
		Selection*         m_selection  = nullptr;
		InspectorRegistry* m_inspectors = nullptr;
		AssetDatabase*     m_database   = nullptr;
		MaterialInspector  m_materials;
	};
}
