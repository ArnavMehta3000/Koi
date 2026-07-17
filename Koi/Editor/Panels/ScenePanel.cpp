#include "Koi/Editor/Panels/ScenePanel.h"
#include "Koi/Engine/Runtime/AppBase.h"
#include "Koi/Engine/Math/Math.h"
#include <imgui.h>
#include <functional>
#include <numbers>

namespace koi
{
	namespace
	{
		constexpr const char* kDragDropPayloadType = "KOI_SCENE_ENTITY";
		constexpr f32 kRadToDeg = 180.0f / static_cast<f32>(std::numbers::pi);
		constexpr f32 kDegToRad = static_cast<f32>(std::numbers::pi) / 180.0f;
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

			Transform& local = m_selected.get_mut<Transform>();

			f32 pos[3]{}, rot[3]{}, scl[3]{};
			store(pos, local.Position);
			store(rot, local.EulerAngles);
			store(scl, local.Scale);
			rot[0] *= kRadToDeg; rot[1] *= kRadToDeg; rot[2] *= kRadToDeg;

			bool posChanged = ImGui::DragFloat3("Position", &local.Position.x, 0.05f);
			bool rotChanged = ImGui::DragFloat3("Rotation", &local.EulerAngles.x, 0.5f);
			bool sclChanged = ImGui::DragFloat3("Scale",    &local.Scale.x, 0.05f);

			//if (posChanged)
			//	local.Position = float3{ pos[0], pos[1], pos[2] };
			//if (rotChanged)
			//	local.EulerAngles = float3{ rot[0] * kDegToRad, rot[1] * kDegToRad, rot[2] * kDegToRad };
			//if (sclChanged)
			//	local.Scale = float3{ scl[0], scl[1], scl[2] };

			if (const MeshRef* meshRef = m_selected.try_get<MeshRef>())
			{
				ImGui::Separator();
				ImGui::TextWrapped("Source: %s", meshRef->SourcePath.c_str());
				if (const GPUMesh* mesh = world->GetMesh(meshRef->Handle))
				{
					ImGui::Text("Draw calls  %d", static_cast<i32>(mesh->DrawItems.size()));
					ImGui::Text("Textures    %d", static_cast<i32>(mesh->Textures.size()));
				}
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
