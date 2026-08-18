#include "Koi/Editor/Panels/InspectorPanel.h"
#include "Koi/Engine/ECS/ComponentRegistry.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Memory/MemoryTracker.h"
#include "Koi/Engine/Runtime/Engine.h"
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
			GfxDevice& device = Engine::Get().GetSubsystem<Renderer>().GetDevice();
			for (const ComponentInfo& component : ComponentRegistry::Get().All())
			{
				if (!component.Add || !component.Has || component.Has(selected)) continue;
				if (ImGui::Selectable(Capitalize(component.Key).c_str()))
					component.Add(selected, device);
			}
			ImGui::EndPopup();
		}

		ImGui::Separator();
		ImGui::Spacing();

		for (const ComponentInfo& component : ComponentRegistry::Get().All())
		{
			if (!component.Draw || !component.Has || !component.Has(selected)) continue;

			ImGui::PushID(component.Key.data(), component.Key.data() + component.Key.size());

			bool keepOpen = true;
			const bool expanded = ImGui::CollapsingHeader(Capitalize(component.Key).c_str(),
				component.Removable ? &keepOpen : nullptr, ImGuiTreeNodeFlags_DefaultOpen);

			if (component.Removable && !keepOpen)
			{
				component.Remove(selected);
			}
			else if (expanded)
			{
				ImGui::Indent();
				component.Draw(selected);
				ImGui::Unindent();
			}

			ImGui::Spacing();
			ImGui::PopID();
		}

		ImGui::End();
	}
}
