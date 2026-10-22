#include "Koi/Editor/Core/PanelManager.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Logging/Logger.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <string_view>

namespace koi::editor
{
	namespace
	{
		class QuickPanel final : public Panel
		{
		public:
			QuickPanel(std::string name, const char* icon, std::function<void()> draw)
				: m_name(std::move(name))
				, m_draw(std::move(draw))
			{
				Info = PanelInfo{ .Name = m_name, .Icon = icon };
			}

			void OnDraw() override { m_draw(); }

			PanelInfo Info;

		private:
			std::string           m_name;  // Info.Name views it, and the profiler zone needs it null-terminated
			std::function<void()> m_draw;
		};
	}

	Panel* PanelManager::Add(std::unique_ptr<Panel> panel, const PanelInfo& info)
	{
		if (Find(info.Name))
		{
			KOI_LOG(Editor, Error, "An editor panel named '{}' already exists; ignoring the second one", info.Name);
			return nullptr;
		}

		KOI_LOG(Editor, Debug, "Registered editor panel '{}'", info.Name);
		panel->m_info = &info;
		return m_panels.emplace_back(std::move(panel)).get();
	}

	void PanelManager::Add(std::string name, const char* icon, std::function<void()> draw)
	{
		auto panel = std::make_unique<QuickPanel>(std::move(name), icon, std::move(draw));
		const PanelInfo& info = panel->Info;
		Add(std::move(panel), info);
	}

	Panel* PanelManager::Find(std::string_view name) const
	{
		for (const std::unique_ptr<Panel>& panel : m_panels)
		{
			if (panel->m_info->Name == name) return panel.get();
		}
		return nullptr;
	}

	const char* PanelManager::Title(const Panel& panel, bool withId)
	{
		const PanelInfo& info = panel.GetInfo();
		m_title.clear();
		if (info.Icon)
		{
			m_title.append(info.Icon).append("  ");
		}
		m_title.append(info.Name);
		if (withId)
		{
			m_title.append("###").append(info.Name);
		}
		return m_title.c_str();
	}

	// [KoiPanels][Open] in imgui.ini, one "Name=0|1" line per closable panel
	void PanelManager::RegisterSettingsHandler()
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

			Panel* panel = static_cast<PanelManager*>(h->UserData)->Find(entry.substr(0, split));
			if (panel && panel->GetInfo().Closable)
			{
				panel->SetOpen(entry.substr(split + 1) != "0");
			}
		};
		handler.WriteAllFn = [](ImGuiContext*, ImGuiSettingsHandler* h, ImGuiTextBuffer* buf)
		{
			buf->append("[KoiPanels][Open]\n");
			for (const std::unique_ptr<Panel>& panel : static_cast<PanelManager*>(h->UserData)->m_panels)
			{
				if (!panel->GetInfo().Closable) continue;

				const std::string_view name = panel->GetInfo().Name;
				buf->appendf("%.*s=%d\n", static_cast<int>(name.size()), name.data(), panel->IsOpen() ? 1 : 0);
			}
			buf->append("\n");
		};
		ImGui::AddSettingsHandler(&handler);
	}

	void PanelManager::SaveState(toml::table& panels) const
	{
		for (const std::unique_ptr<Panel>& panel : m_panels)
		{
			toml::table state;
			panel->SaveState(state);
			if (!state.empty())
			{
				panels.insert_or_assign(panel->GetInfo().Name, std::move(state));
			}
		}
	}

	void PanelManager::LoadState(const toml::table& panels)
	{
		static const toml::table kEmpty;
		for (const std::unique_ptr<Panel>& panel : m_panels)
		{
			const toml::table* state = panels[panel->GetInfo().Name].as_table();
			panel->LoadState(state ? *state : kEmpty);
		}
	}

	void PanelManager::DrawAll()
	{
		ZoneScoped;
		for (std::unique_ptr<Panel>& panel : m_panels)
		{
			if (!panel->IsOpen()) continue;

			const PanelInfo& info = panel->GetInfo();
			ZoneTransientN(panelZone, info.Name.data(), true);  // every name is a literal or a std::string
			panel->OnUpdate();

			if (std::exchange(panel->m_focusPending, false))
			{
				ImGui::SetNextWindowFocus();
			}

			if (!info.Padding)
			{
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
			}

			bool open = true;
			const bool visible = ImGui::Begin(Title(*panel, true), info.Closable ? &open : nullptr, info.WindowFlags);
			if (!info.Padding)
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

	void PanelManager::DrawMenuItems()
	{
		for (std::unique_ptr<Panel>& panel : m_panels)
		{
			if (!panel->GetInfo().Closable) continue;

			const bool open = panel->IsOpen();
			if (ImGui::MenuItem(Title(*panel, false), nullptr, open))
			{
				panel->SetOpen(!open);
				ImGui::MarkIniSettingsDirty();
			}
		}
	}
}
