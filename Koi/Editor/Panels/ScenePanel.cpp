#include "Koi/Editor/Panels/ScenePanel.h"
#include "Koi/Engine/Runtime/AppBase.h"
#include "Koi/Engine/ECS/ComponentRegistry.h"
#include "Koi/Engine/Memory/MemoryTracker.h"
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
		KOI_MEM_TAG_SCOPE(MemTag::Editor);
		World* world = m_game->GetEditorWorld();
		if (!world) return;

		ECS& ecs = world->GetECS();

		// Make modifications at end of draw
		Entity pendingChild;
		Entity pendingParent;

		Entity& selected = m_selection->Selected;

		auto DrawNode = [&](this auto&& self, Entity entity)
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
			bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(entity.id())), flags, "%s", label);

			if (ImGui::IsItemClicked())
				selected = entity;

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
		if (selected.is_alive())
		{
			const char* name = selected.name().length() > 0 ? selected.name().c_str() : "(unnamed)";
			ImGui::TextUnformatted(name);
			ImGui::Separator();

			// Draw components
			for (const ComponentInfo& component : ComponentRegistry::Get().All())
			{
				if (!component.Draw) continue;

				ImGui::PushID(component.Key.data(), component.Key.data() + component.Key.size());
				component.Draw(selected);
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
