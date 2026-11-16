#include "Koi/Editor/Panels/InspectorPanel.h"
#include "Koi/Editor/Assets/AssetDatabase.h"
#include "Koi/Editor/Core/EditorContext.h"
#include "Koi/Editor/Inspectors/InspectorRegistry.h"
#include "Koi/Editor/UI/Widgets.h"
#include "Koi/Editor/Undo/UndoStack.h"
#include "Koi/Engine/ECS/ComponentRegistry.h"
#include <IconsFontAwesome6.h>
#include <imgui.h>
#include <cctype>
#include <string>

namespace koi::editor
{
	namespace
	{
		// "RigidBody" -> "Rigid Body"
		std::string DisplayName(std::string_view key)
		{
			std::string s;
			for (size_t i = 0; i < key.size(); ++i)
			{
				const auto c = static_cast<unsigned char>(key[i]);
				if (i > 0 && std::isupper(c) && std::islower(static_cast<unsigned char>(key[i - 1]))) s += ' ';
				s += static_cast<char>(c);
			}
			return s;
		}
	}

	InspectorPanel::InspectorPanel(EditorContext* ctx)
		: m_selection(ctx->Get<Selection>())
		, m_inspectors(ctx->Get<InspectorRegistry>())
		, m_database(ctx->Get<AssetDatabase>())
		, m_undo(ctx->Get<UndoStack>())
	{
	}

	UndoScope InspectorPanel::WatchComponent(std::string_view verb, std::string_view key, Entity e) const
	{
		IComponentInspector* inspector = m_inspectors->Find(key);
		if (!inspector) return UndoScope(nullptr, nullptr);

		return inspector->Watch(*m_undo, std::format("{} {}", verb, DisplayName(key)), e);
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

		if (!selected.IsAlive())
		{
			ImGui::TextDisabled("Select a node");
			return;
		}

		const std::string nameText(selected.GetName());
		const char* name = nameText.empty() ? "(unnamed)" : nameText.c_str();
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
				if (ImGui::Selectable(DisplayName(type->Key()).c_str()))
				{
					UndoScope watch = WatchComponent("Add", type->Key(), selected);
					type->Add(selected);
				}
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

			const bool expanded = utils::SectionHeader(DisplayName(key).c_str(), type->Removable());

			bool remove = false;
			if (utils::BeginSectionMenu())
			{
				remove = ImGui::MenuItem(ICON_FA_TRASH "  Remove Component");
				ImGui::EndPopup();
			}

			if (remove)
			{
				UndoScope watch = WatchComponent("Remove", key, selected);
				type->Remove(selected);
			}
			else if (expanded)
			{
				// A component is free to have no inspector; it still serializes and can be removed, just not undoably
				if (IComponentInspector* inspector = m_inspectors->Find(key))
				{
					UndoScope watch = inspector->Watch(*m_undo, "Edit " + DisplayName(key), selected);
					inspector->Draw(selected);
				}
				else
				{
					ImGui::TextDisabled("No inspector registered");
				}
			}

			ImGui::Spacing();
			ImGui::PopID();
		}
	}
}
