#include "Koi/Editor/Panels/WorldOutlinePanel.h"
#include "Koi/Engine/Runtime/AppBase.h"
#include <imgui.h>
#include <imgui_stdlib.h>

namespace koi::editor
{
	namespace
	{
		constexpr const char* kDragDropPayloadType = "KOI_SCENE_ENTITY";
	}

	void WorldOutlinePanel::OnDraw()
	{
		ECS& ecs = m_game->GetWorld().GetECS();

		// Make modifications at end of draw
		Entity pendingChild;
		Entity pendingParent;

		Entity& selected = m_selection->Selected;

		auto DrawNode = [&](this auto&& self, Entity entity) -> void
		{
			bool hasChildren = false;
			entity.children([&](Entity) { hasChildren = true; });

			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
										| ImGuiTreeNodeFlags_SpanAvailWidth
										| ImGuiTreeNodeFlags_DefaultOpen;

			if (!hasChildren)
				flags |= ImGuiTreeNodeFlags_Leaf;

			if (selected.is_alive() && selected.id() == entity.id())
				flags |= ImGuiTreeNodeFlags_Selected;

			const char* label = entity.name().length() > 0 ? entity.name().c_str() : "<unnamed>";
			void* nodeId = reinterpret_cast<void*>(static_cast<intptr_t>(entity.id()));

			const bool renamingThis = m_renaming == entity.id();
			bool open;

			if (renamingThis)
			{
				open = ImGui::TreeNodeEx(nodeId, flags, "");
				ImGui::SameLine();

				if (m_focusRename)
				{
					ImGui::SetKeyboardFocusHere();
					m_focusRename = false;
				}

				ImGui::SetNextItemWidth(160.0f);
				const bool commit = ImGui::InputText("##rename", &m_renameBuffer,
					ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

				if (ImGui::IsKeyPressed(ImGuiKey_Escape))
				{
					m_renaming = 0;
				}
				else if (commit || ImGui::IsItemDeactivatedAfterEdit())
				{
					if (!m_renameBuffer.empty())
						entity.set_name(m_renameBuffer.c_str());
					m_renaming = 0;
				}
				else if (ImGui::IsItemDeactivated())
				{
					m_renaming = 0;
				}
			}
			else
			{
				open = ImGui::TreeNodeEx(nodeId, flags, "%s", label);

				if (ImGui::IsItemClicked())
					selected = entity;

				if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered())
				{
					m_renaming     = entity.id();
					m_renameBuffer = label;
					m_focusRename  = true;
				}

				if (ImGui::BeginDragDropSource())
				{
					EntityID id = entity.id();
					ImGui::SetDragDropPayload(kDragDropPayloadType, &id, sizeof(id));
					ImGui::TextUnformatted(label);
					ImGui::EndDragDropSource();
				}

				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kDragDropPayloadType))
					{
						EntityID draggedId = *static_cast<EntityID*>(payload->Data);
						Entity dragged = ecs.entity(draggedId);
						if (dragged.is_alive() && draggedId != entity.id())
						{
							pendingChild  = dragged;
							pendingParent = entity;
						}
					}
					ImGui::EndDragDropTarget();
				}
			}

			if (open)
			{
				// Recurse
				entity.children(self);
				ImGui::TreePop();
			}
		};

		ImGui::Begin("World Outline");

		// F2 renames the current selection wherever it ends up in the tree below
		if (selected.is_alive() && ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_F2))
		{
			m_renaming     = selected.id();
			m_renameBuffer = selected.name().length() > 0 ? selected.name().c_str() : "";
			m_focusRename  = true;
		}

		if (ImGui::Button("Add"))
		{
			Entity e = ecs.entity();
			e.set_name(("Entity_" + std::to_string(static_cast<u64>(e.id()))).c_str());
			e.set<Transform>({});
			if (selected.is_alive())
				e.child_of(selected);
			selected = e;
		}
		ImGui::SameLine();
		ImGui::BeginDisabled(!selected.is_alive());
		if (ImGui::Button("Remove"))
		{
			selected.destruct();
			selected = Entity{};
		}
		ImGui::EndDisabled();

		ImGui::Separator();

		ecs.each<Transform>([&](Entity e, Transform&)
		{
			if (!e.parent())
				DrawNode(e);
		});

		if (pendingChild.is_alive())
			pendingChild.child_of(pendingParent);

		ImGui::End();
	}
}
