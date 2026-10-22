#include "Koi/Editor/Panels/InspectorPanel.h"
#include "Koi/Editor/Assets/AssetDatabase.h"
#include "Koi/Editor/Inspectors/InspectorRegistry.h"
#include "Koi/Editor/UI/Widgets.h"
#include "Koi/Engine/ECS/ComponentRegistry.h"
#include <IconsFontAwesome6.h>
#include <imgui.h>
#include <cctype>

namespace koi::editor
{
	namespace
	{
		std::string Capitalize(std::string_view key)
		{
			std::string s(key);
			if (!s.empty()) s[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
			return s;
		}
	}

	void InspectorPanel::OnDraw()
	{
		const Entity selected = m_selection->GetEntity();

		// Selection is exclusive, so an asset here means no entity is selected
		if (const AssetEntry* asset = m_database->Find(m_selection->GetAsset());
		    asset && asset->Kind == AssetKind::Material)
		{
			m_materials.Draw(*m_database, *asset);
			return;
		}

		if (!selected.is_alive())
		{
			ImGui::TextDisabled("Select a node");
			return;
		}

		const char* name = selected.name().length() > 0 ? selected.name().c_str() : "(unnamed)";
		ImGui::AlignTextToFramePadding();
		utils::PushHeadingFont();
		ImGui::TextUnformatted(name);
		utils::PopHeadingFont();

		constexpr const char* kAddLabel = ICON_FA_PLUS "  Add Component";
		const f32 addWidth = ImGui::CalcTextSize(kAddLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f;
		ImGui::SameLine(ImGui::GetContentRegionAvail().x - addWidth + ImGui::GetCursorPosX());
		if (ImGui::Button(kAddLabel))
			ImGui::OpenPopup("##addComponent");

		if (ImGui::BeginPopup("##addComponent"))
		{
			for (const std::unique_ptr<IComponentType>& type : ComponentRegistry::Get().All())
			{
				if (type->Has(selected)) continue;
				if (ImGui::Selectable(Capitalize(type->Key()).c_str()))
					type->Add(selected);
			}
			ImGui::EndPopup();
		}

		ImGui::Separator();
		ImGui::Spacing();

		for (const std::unique_ptr<IComponentType>& type : ComponentRegistry::Get().All())
		{
			if (!type->Has(selected)) continue;

			const std::string_view key = type->Key();
			ImGui::PushID(key.data(), key.data() + key.size());

			const bool expanded = utils::SectionHeader(Capitalize(key).c_str(), type->Removable());

			bool remove = false;
			if (utils::BeginSectionMenu())
			{
				remove = ImGui::MenuItem(ICON_FA_TRASH "  Remove Component");
				ImGui::EndPopup();
			}

			if (remove)
			{
				type->Remove(selected);
			}
			else if (expanded)
			{
				// A component is free to have no inspector; it still serializes and can be removed
				if (IComponentInspector* inspector = m_inspectors->Find(key))
					inspector->Draw(selected);
				else
					ImGui::TextDisabled("No inspector registered");
			}

			ImGui::Spacing();
			ImGui::PopID();
		}
	}
}
