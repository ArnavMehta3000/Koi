#include "Koi/Editor/Panels/ScenePanel.h"
#include "Koi/Engine/Runtime/AppBase.h"
#include "Koi/Engine/Runtime/Engine.h"
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

			if (m_eulerFor != m_selected.id())
			{
				m_eulerDeg = local.Rotation.ToEuler() * kRadToDeg;
				m_eulerFor = m_selected.id();
			}

			ImGui::DragFloat3("Position", &local.Position.x, 0.05f, ImGuiSliderFlags_ColorMarkers);
			if (ImGui::DragFloat3("Rotation", &m_eulerDeg.x, 0.5f))
			{
				local.Rotation = Quaternion::CreateFromYawPitchRoll(
					m_eulerDeg.y * kDegToRad, m_eulerDeg.x * kDegToRad, m_eulerDeg.z * kDegToRad);
			}
			ImGui::DragFloat3("Scale", &local.Scale.x, 0.05f, ImGuiSliderFlags_ColorMarkers);

			if (const MeshRef* meshRef = m_selected.try_get<MeshRef>())
			{
				ImGui::Separator();
				const std::string_view source = Engine::Get().GetAssets().Cache<GPUMesh>().GetPath(meshRef->Handle);
				ImGui::TextWrapped("Source: %.*s", static_cast<int>(source.size()), source.data());
				if (const GPUMesh* mesh = world->GetMesh(meshRef->Handle))
				{
					ImGui::Text("Draw calls  %d", static_cast<i32>(mesh->DrawItems.size()));
					ImGui::Text("Textures    %d", static_cast<i32>(mesh->Textures.size()));
				}
			}

			if (m_selected.has<CameraComponent>())
			{
				CameraComponent& cam = m_selected.get_mut<CameraComponent>();
				ImGui::Separator();
				ImGui::TextUnformatted("Camera");

				f32 fovDeg = cam.FovY * kRadToDeg;
				if (ImGui::DragFloat("FOV", &fovDeg, 0.5f, 1.0f, 179.0f))
					cam.FovY = fovDeg * kDegToRad;
				ImGui::DragFloat("Near", &cam.Near, 0.01f, 0.001f, cam.Far);
				ImGui::DragFloat("Far", &cam.Far, 1.0f, cam.Near, 100000.0f);
				ImGui::Checkbox("Primary", &cam.Primary);
			}

			if (m_selected.has<Light>())
			{
				Light& light = m_selected.get_mut<Light>();
				ImGui::Separator();
				ImGui::TextUnformatted("Light");

				const char* types[] = { "Directional", "Point" };
				int type = static_cast<int>(light.Type);
				if (ImGui::Combo("Type", &type, types, IM_ARRAYSIZE(types)))
					light.Type = static_cast<LightType>(type);

				ImGui::ColorEdit3("Color", &light.Color.x);
				ImGui::DragFloat("Intensity", &light.Intensity, 0.05f, 0.0f, 100.0f);
				if (light.Type == LightType::Point)
					ImGui::DragFloat("Range", &light.Range, 0.1f, 0.0f, 1000.0f);
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
