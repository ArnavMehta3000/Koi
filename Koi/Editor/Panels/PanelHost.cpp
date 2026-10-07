#include "Koi/Editor/Panels/PanelHost.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Logging/Logger.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <string_view>

namespace koi::editor
{
	void PanelHost::Add(std::unique_ptr<IEditorPanel> panel)
	{
		KOI_LOG_DEBUG("Registered editor panel '{}'", panel->GetName());
		m_panels.push_back(std::move(panel));
	}

	IEditorPanel* PanelHost::Find(std::string_view name) const
	{
		for (const std::unique_ptr<IEditorPanel>& panel : m_panels)
		{
			if (panel->GetName() == name) return panel.get();
		}
		return nullptr;
	}

	const char* PanelHost::Title(const IEditorPanel& panel, bool withId)
	{
		const std::string_view name = panel.GetName();
		m_title.clear();
		if (const char* icon = panel.GetIcon())
		{
			m_title.append(icon).append("  ");
		}
		m_title.append(name);
		if (withId)
		{
			m_title.append("###").append(name);
		}
		return m_title.c_str();
	}

	// [KoiPanels][Open] in imgui.ini, one "Name=0|1" line per closable panel
	void PanelHost::RegisterSettingsHandler()
	{
		ImGuiSettingsHandler handler;
		handler.TypeName   = "KoiPanels";
		handler.TypeHash   = ImHashStr("KoiPanels");
		handler.UserData   = this;
		handler.ReadOpenFn = [](ImGuiContext*, ImGuiSettingsHandler*, const char* name) -> void*
		{
			return std::string_view(name) == "Open" ? reinterpret_cast<void*>(1) : nullptr;
		};
		handler.ReadLineFn = [](ImGuiContext*, ImGuiSettingsHandler* h, void*, const char* line)
		{
			const std::string_view entry(line);
			const size_t split = entry.rfind('=');
			if (split == std::string_view::npos) return;

			IEditorPanel* panel = static_cast<PanelHost*>(h->UserData)->Find(entry.substr(0, split));
			if (panel && panel->IsClosable())
			{
				panel->SetOpen(entry.substr(split + 1) != "0");
			}
		};
		handler.WriteAllFn = [](ImGuiContext*, ImGuiSettingsHandler* h, ImGuiTextBuffer* buf)
		{
			buf->append("[KoiPanels][Open]\n");
			for (const std::unique_ptr<IEditorPanel>& panel : static_cast<PanelHost*>(h->UserData)->m_panels)
			{
				if (!panel->IsClosable()) continue;

				const std::string_view name = panel->GetName();
				buf->appendf("%.*s=%d\n", static_cast<int>(name.size()), name.data(), panel->IsOpen() ? 1 : 0);
			}
			buf->append("\n");
		};
		ImGui::AddSettingsHandler(&handler);
	}

	void PanelHost::DrawAll()
	{
		ZoneScoped;
		for (std::unique_ptr<IEditorPanel>& panel : m_panels)
		{
			if (!panel->IsOpen()) continue;

			ZoneTransientN(panelZone, panel->GetName().data(), true);  // every GetName() is a literal
			panel->OnUpdate();

			const bool padded = panel->HasWindowPadding();
			if (!padded)
			{
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
			}

			bool open = true;
			const bool visible = ImGui::Begin(Title(*panel, true), panel->IsClosable() ? &open : nullptr, panel->GetWindowFlags());
			if (!padded)
			{
				ImGui::PopStyleVar();
			}

			if (visible)
			{
				panel->OnDraw();
			}
			ImGui::End();

			if (!open)
			{
				panel->SetOpen(false);
				ImGui::MarkIniSettingsDirty();
			}
		}
	}

	void PanelHost::DrawMenuItems()
	{
		for (std::unique_ptr<IEditorPanel>& panel : m_panels)
		{
			if (!panel->IsClosable()) continue;

			const bool open = panel->IsOpen();
			if (ImGui::MenuItem(Title(*panel, false), nullptr, open))
			{
				panel->SetOpen(!open);
				ImGui::MarkIniSettingsDirty();
			}
		}
	}
}
