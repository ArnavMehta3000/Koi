#include "Koi/Editor/ImGui/EditorPalette.h"

namespace koi::editor
{
	const EditorPalette* EditorPalette::s_active = nullptr;

	const EditorPalette& EditorPalette::Get()
	{
		return *s_active;
	}

	void EditorPalette::SetActive(const EditorPalette& palette)
	{
		s_active = &palette;
	}
}
