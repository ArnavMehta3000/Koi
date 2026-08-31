#include "Koi/Editor/Panels/InspectorPanel.h"
#include "Koi/Editor/Inspectors/InspectorRegistry.h"
#include "Koi/Engine/ECS/ComponentRegistry.h"
#include "Koi/Engine/Memory/MemoryTracker.h"
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
		KOI_MEM_TAG_SCOPE(MemTag::Editor);
		Entity selected = m_selection->Selected;

		ImGui::Begin("Inspector");

		if (!selected.is_alive())
		{
			ImGui::TextDisabled("Select a node");
			ImGui::End();
			return;
		}

		const char* name = selected.name().length() > 0 ? selected.name().c_str() : "(unnamed)";
		ImGui::TextUnformatted(name);

		const f32 addWidth = ImGui::CalcTextSize("Add Component").x + ImGui::GetStyle().FramePadding.x * 2.0f;
		ImGui::SameLine(ImGui::GetContentRegionAvail().x - addWidth + ImGui::GetCursorPosX());
		if (ImGui::Button("Add Component"))
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

			bool keepOpen = true;
			const bool expanded = ImGui::CollapsingHeader(Capitalize(key).c_str(),
				type->Removable() ? &keepOpen : nullptr, ImGuiTreeNodeFlags_DefaultOpen);

			if (type->Removable() && !keepOpen)
			{
				type->Remove(selected);
			}
			else if (expanded)
			{
				ImGui::Indent();

				// A component is free to have no inspector; it still serializes and can be removed
				if (IComponentInspector* inspector = m_inspectors->Find(key))
					inspector->Draw(selected);
				else
					ImGui::TextDisabled("No inspector registered");

				ImGui::Unindent();
			}

			ImGui::Spacing();
			ImGui::PopID();
		}

		ImGui::End();
	}
}
