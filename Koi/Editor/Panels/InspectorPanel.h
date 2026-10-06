#pragma once
#include "Koi/Engine/Interop/EditorPanel.h"
#include "Koi/Editor/Inspectors/MaterialInspector.h"
#include "Koi/Editor/Selection.h"

namespace koi::editor
{
	class InspectorRegistry;
	class AssetDatabase;

	class InspectorPanel : public IEditorPanel
	{
	public:
		InspectorPanel(Selection& selection, InspectorRegistry& inspectors, AssetDatabase& database)
			: m_selection(&selection)
			, m_inspectors(&inspectors)
			, m_database(&database)
		{
		}

		[[nodiscard]] std::string_view GetName() const override { return "Inspector"; }
		[[nodiscard]] bool IsClosable() const override { return false; }
		void OnDraw() override;

	private:
		Selection*         m_selection  = nullptr;
		InspectorRegistry* m_inspectors = nullptr;
		AssetDatabase*     m_database   = nullptr;
		MaterialInspector  m_materials;
	};
}
