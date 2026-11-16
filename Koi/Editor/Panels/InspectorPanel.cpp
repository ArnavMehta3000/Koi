#include "Koi/Editor/Panels/InspectorPanel.h"
#include "Koi/Editor/Assets/AssetDatabase.h"
#include "Koi/Editor/Core/EditorContext.h"
#include "Koi/Editor/Inspectors/InspectorRegistry.h"
#include "Koi/Editor/UI/Widgets.h"
#include "Koi/Editor/Undo/UndoStack.h"
#include "Koi/Engine/ECS/ComponentRegistry.h"
#include <IconsFontAwesome6.h>
#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <cfloat>
#include <format>
#include <string>
#include <vector>

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

		bool ContainsNoCase(std::string_view text, std::string_view needle)
		{
			const auto lower = [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); };
			return needle.empty() || !std::ranges::search(text, needle, {}, lower, lower).empty();
		}

		const char* CategoryIcon(std::string_view category)
		{
			if (category == "Rendering") return ICON_FA_CUBE;
			if (category == "Physics")   return ICON_FA_ATOM;
			if (category == "Gameplay")  return ICON_FA_GAMEPAD;
			return ICON_FA_PUZZLE_PIECE;
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

	void InspectorPanel::DrawAddComponentMenu(Entity e)
	{
		const auto all = ComponentRegistry::Get().All();

		if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
		ImGui::SetNextItemWidth(-FLT_MIN);
		utils::SearchBar("##search", m_addFilter, "Search components...");
		const bool submit = ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter);
		ImGui::Separator();

		// Headings only while browsing; a search is one flat list in draw order
		std::vector<std::string_view> categories;
		for (const std::unique_ptr<IComponentType>& type : all)
		{
			if (std::ranges::find(categories, type->Category()) == categories.end()) categories.push_back(type->Category());
		}
		if (!m_addFilter.empty()) categories = { std::string_view() };

		const IComponentType* toAdd = nullptr;
		bool anyShown = false;

		ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(FLT_MAX, ImGui::GetFrameHeightWithSpacing() * 14.0f));
		ImGui::BeginChild("##components", ImVec2(0.0f, 0.0f), ImGuiChildFlags_AutoResizeY);
		for (const std::string_view category : categories)
		{
			bool headed = false;
			for (const std::unique_ptr<IComponentType>& type : all)
			{
				if (!type->Removable()) continue;  // required ones are always there
				if (!category.empty() && type->Category() != category) continue;

				const std::string name = DisplayName(type->Key());
				if (!ContainsNoCase(name, m_addFilter) && !ContainsNoCase(type->Category(), m_addFilter)) continue;

				if (!category.empty() && !headed)
				{
					ImGui::SeparatorText(std::format("{}  {}", CategoryIcon(category), category).c_str());
					headed = true;
				}

				const bool has = type->Has(e);
				anyShown = true;

				const std::string label = std::format("{}  {}##{}", CategoryIcon(type->Category()), name, type->Key());
				if (ImGui::Selectable(label.c_str(), false, has ? ImGuiSelectableFlags_Disabled : 0)) toAdd = type.get();

				if (has)
				{
					const ImVec2 min = ImGui::GetItemRectMin();
					const ImVec2 max = ImGui::GetItemRectMax();
					const f32 width = ImGui::CalcTextSize(ICON_FA_CHECK).x;
					ImGui::GetWindowDrawList()->AddText(ImVec2(max.x - width - ImGui::GetStyle().FramePadding.x, min.y),
						ImGui::GetColorU32(ImGuiCol_TextDisabled), ICON_FA_CHECK);
				}
				else if (submit && !toAdd && !m_addFilter.empty())
				{
					toAdd = type.get();
				}
			}
		}

		if (!anyShown) ImGui::TextDisabled("No components match");
		ImGui::EndChild();

		if (toAdd)
		{
			UndoScope watch = WatchComponent("Add", toAdd->Key(), e);
			toAdd->Add(e);
			ImGui::CloseCurrentPopup();
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
		{
			m_addFilter.clear();
			ImGui::OpenPopup("##addComponent");
		}

		ImGui::SetNextWindowSizeConstraints(ImVec2(ImGui::GetFontSize() * 16.0f, 0.0f), ImVec2(FLT_MAX, FLT_MAX));
		if (ImGui::BeginPopup("##addComponent"))
		{
			DrawAddComponentMenu(selected);
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
