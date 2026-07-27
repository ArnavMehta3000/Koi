#include "Koi/Editor/Panels/ScenePanel.h"
#include "Koi/Engine/Runtime/AppBase.h"
#include "Koi/Engine/ECS/ComponentRegistry.h"
#include <imgui.h>
#include <functional>

namespace koi
{
	namespace
	{
		constexpr const char* kDragDropPayloadType = "KOI_SCENE_ENTITY";
	}

	void ScenePanel::OnDraw()
	{
		World* world = m_game->GetEditorWorld();
		if (!world) return;

		ECS& ecs = world->GetECS();

		// Make modifications at end of draw
		Entity pendingChild;
		Entity pendingParent;

		auto DrawNode = [&](this auto&& self, Entity entity)
		{
			bool hasChildren = false;
			entity.children([&](Entity) { hasChildren = true; });

			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
										| ImGuiTreeNodeFlags_SpanAvailWidth
										| ImGuiTreeNodeFlags_DefaultOpen;

			if (!hasChildren)
				flags |= ImGuiTreeNodeFlags_Leaf;

			if (m_selected.is_alive() && m_selected.id() == entity.id())
				flags |= ImGuiTreeNodeFlags_Selected;

			const char* label = entity.name().length() > 0 ? entity.name().c_str() : "<unnamed>";
			bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(entity.id())), flags, "%s", label);

			if (ImGui::IsItemClicked())
				m_selected = entity;

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

			if (open)
			{
				// Recurse
				entity.children(self);
				ImGui::TreePop();
			}
		};

		ImGui::Begin("Scene");

		if (ImGui::Button("Add"))
		{
			// TODO: Replace with create function from the world
			Entity e = ecs.entity();
			e.set<Transform>({});
			if (m_selected.is_alive())
				e.child_of(m_selected);
			m_selected = e;
		}
		ImGui::SameLine();
		ImGui::BeginDisabled(!m_selected.is_alive());
		if (ImGui::Button("Remove"))
		{
			m_selected.destruct();
			m_selected = Entity{};
		}
		ImGui::EndDisabled();

		ImGui::BeginChild("##tree", ImVec2(220.0f, 0.0f), ImGuiChildFlags_Borders);
		ecs.each<Transform>([&](Entity e, Transform&)
		{
			if (!e.parent())
				DrawNode(e);
		});
		ImGui::EndChild();

		if (pendingChild.is_alive())
			pendingChild.child_of(pendingParent);

		ImGui::SameLine();

		ImGui::BeginChild("##props", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
		if (m_selected.is_alive())
		{
			const char* name = m_selected.name().length() > 0 ? m_selected.name().c_str() : "(unnamed)";
			ImGui::TextUnformatted(name);
			ImGui::Separator();

			// Draw components
			for (const ComponentInfo& component : ComponentRegistry::Get().All())
			{
				if (!component.Draw) continue;

				ImGui::PushID(component.Key.data(), component.Key.data() + component.Key.size());
				component.Draw(m_selected);
				ImGui::PopID();
			}
		}
		else
		{
			ImGui::TextDisabled("Select a node");
		}
		ImGui::EndChild();

		ImGui::End();
	}
}
