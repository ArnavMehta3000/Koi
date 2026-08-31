#pragma once
#include "Koi/Engine/Interop/EditorPanel.h"
#include "Koi/Editor/Selection.h"

namespace koi::editor
{
	class InspectorRegistry;

	class InspectorPanel : public IEditorPanel
	{
	public:
		InspectorPanel(Selection& selection, InspectorRegistry& inspectors)
			: m_selection(&selection)
			, m_inspectors(&inspectors)
		{
		}

		[[nodiscard]] std::string_view GetName() const override { return "Inspector"; }
		void OnDraw() override;

	private:
		Selection*         m_selection  = nullptr;
		InspectorRegistry* m_inspectors = nullptr;
	};
}
