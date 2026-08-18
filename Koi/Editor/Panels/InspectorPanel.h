#pragma once
#include "Koi/Engine/Interop/EditorPanel.h"
#include "Koi/Editor/Selection.h"

namespace koi::editor
{
	class InspectorPanel : public IEditorPanel
	{
	public:
		explicit InspectorPanel(Selection& selection) : m_selection(&selection) {}

		[[nodiscard]] std::string_view GetName() const override { return "Inspector"; }
		void OnDraw() override;

	private:
		Selection* m_selection = nullptr;
	};
}
