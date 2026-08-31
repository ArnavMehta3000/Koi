#include "Koi/Editor/Panels/PanelHost.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Logging/Logger.h"
#include <imgui.h>

namespace koi::editor
{
	void PanelHost::Add(std::unique_ptr<IEditorPanel> panel)
	{
		KOI_LOG_DEBUG("Registered editor panel '{}'", panel->GetName());
		m_panels.push_back(std::move(panel));
	}

	void PanelHost::DrawAll()
	{
		ZoneScoped;
		for (std::unique_ptr<IEditorPanel>& panel : m_panels)
		{
			if (!panel->IsOpen()) continue;

			ZoneTransientN(panelZone, panel->GetName().data(), true);  // every GetName() is a literal
			panel->OnDraw();
		}
	}

	void PanelHost::DrawMenuItems()
	{
		for (std::unique_ptr<IEditorPanel>& panel : m_panels)
		{
			if (!panel->IsClosable()) continue;

			const bool open = panel->IsOpen();
			if (ImGui::MenuItem(panel->GetName().data(), nullptr, open))
			{
				panel->SetOpen(!open);
			}
		}
	}
}
