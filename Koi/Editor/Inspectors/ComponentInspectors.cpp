#include "Koi/Editor/Inspectors/ComponentInspectors.h"
#include "Koi/Editor/Inspectors/InspectorRegistry.h"
#include "Koi/Engine/Assets/AssetTypes.h"
#include "Koi/Engine/Assets/MeshAsset.h"
#include "Koi/Engine/ECS/Components.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Math/Transform.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <imgui.h>
#include <numbers>

namespace koi::editor
{
	namespace
	{
		constexpr f32 kRadToDeg = 180.0f / static_cast<f32>(std::numbers::pi);
		constexpr f32 kDegToRad = static_cast<f32>(std::numbers::pi) / 180.0f;

		AssetCache<GPUMesh>& Meshes() { return Engine::Get().GetSubsystem<AssetRegistry>().Cache<GPUMesh>(); }
		GfxDevice&           Device() { return Engine::Get().GetSubsystem<Renderer>().GetDevice(); }

		// Acquire before remove: if the new key is the one already held, the extra reference
		// keeps the mesh resident across the removal that releases the old handle
		static void ReacquireMesh(Entity e, const AssetKey& key)
		{
			const MeshHandle handle = Meshes().Acquire(Device(), key);
			if (!handle) return;

			e.remove<MeshRef>();  // World's on_remove hook releases whatever was there
			e.set<MeshRef>({ handle });
		}
	}

	void TransformInspector::Draw(Entity e)
	{
		Transform* local = e.try_get_mut<Transform>();
		if (!local) return;

		// Also re-derive when something else moved the entity (the viewport gizmo), or the widget would keep showing the angles from before the drag
		if (m_eulerFor != e.id() || m_eulerRot != local->Rotation)
		{
			m_eulerDeg = local->Rotation.ToEuler() * kRadToDeg;
			m_eulerFor = e.id();
			m_eulerRot = local->Rotation;
		}

		ImGui::DragFloat3("Position", &local->Position.x, 0.05f, ImGuiSliderFlags_ColorMarkers);

		if (ImGui::DragFloat3("Rotation", &m_eulerDeg.x, 0.5f))
		{
			local->Rotation = Quaternion::CreateFromYawPitchRoll(
				m_eulerDeg.y * kDegToRad, m_eulerDeg.x * kDegToRad, m_eulerDeg.z * kDegToRad);
			m_eulerRot = local->Rotation;
		}
		ImGui::DragFloat3("Scale", &local->Scale.x, 0.05f, ImGuiSliderFlags_ColorMarkers);
	}

	void MeshInspector::Draw(Entity e)
	{
		const MeshRef* meshRef = e.try_get<MeshRef>();
		if (!meshRef) return;

		const std::string_view source = Meshes().GetPath(meshRef->Handle);

		// Doubles as the drop zone for a mesh dragged out of the Assets panel
		const std::string label = source.empty() ? std::string("(none)") : std::string(source);
		ImGui::Button(label.c_str(), ImVec2(-FLT_MIN, 0.0f));
		ImGui::SetItemTooltip("Drag a mesh here from the Assets panel");

		bool replaced = false;
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kAssetDragDropType))
			{
				const std::string dropped(static_cast<const char*>(payload->Data));

				if (const AssetTypeInfo* type = FindAssetTypeForFile(dropped); type->Kind == AssetKind::Mesh)
				{
					ReacquireMesh(e, AssetKey(dropped));
					replaced = true;
				}
				else
				{
					KOI_LOG_WARN("Dropped asset is not a mesh: {}", dropped);
				}
			}
			ImGui::EndDragDropTarget();
		}

		// The old handle was just released, so meshRef and source are stale - resume next frame
		if (replaced) return;

		if (const GPUMesh* mesh = Meshes().Get(meshRef->Handle))
		{
			ImGui::Text("Draw calls  %d", static_cast<i32>(mesh->DrawItems.size()));
			ImGui::Text("Materials   %d", static_cast<i32>(mesh->Materials.size()));
		}

	}

	void CameraInspector::Draw(Entity e)
	{
		CameraComponent* cam = e.try_get_mut<CameraComponent>();
		if (!cam) return;

		f32 fovDeg = cam->FovY * kRadToDeg;
		if (ImGui::DragFloat("FOV", &fovDeg, 0.5f, 1.0f, 179.0f))
		{
			cam->FovY = fovDeg * kDegToRad;
		}

		ImGui::DragFloat("Near", &cam->Near, 0.01f, 0.001f, cam->Far);
		ImGui::DragFloat("Far", &cam->Far, 1.0f, cam->Near, 100000.0f);
		ImGui::Checkbox("Primary", &cam->Primary);
	}

	void LightInspector::Draw(Entity e)
	{
		Light* light = e.try_get_mut<Light>();
		if (!light) return;

		const char* types[] = { "Directional", "Point" };
		int type = static_cast<int>(light->Type);
		if (ImGui::Combo("Type", &type, types, IM_ARRAYSIZE(types)))
		{
			light->Type = static_cast<LightType>(type);
		}

		ImGui::ColorEdit3("Color", &light->Color.x);
		ImGui::DragFloat("Intensity", &light->Intensity, 0.05f, 0.0f, 100.0f);
		if (light->Type == LightType::Point)
		{
			ImGui::DragFloat("Range", &light->Range, 0.1f, 0.0f, 1000.0f);
		}

		if (light->Type == LightType::Directional)
		{
			ImGui::Checkbox("Casts Shadows", &light->CastsShadows);
			if (light->CastsShadows)
			{
				ImGui::DragFloat("Shadow Bias", &light->ShadowBias, 0.0001f, 0.0f, 0.05f, "%.4f");
			}
		}
	}

	void RegisterBuiltinInspectors(InspectorRegistry& registry)
	{
		registry.Emplace<TransformInspector>();
		registry.Emplace<MeshInspector>();
		registry.Emplace<CameraInspector>();
		registry.Emplace<LightInspector>();
	}
}
