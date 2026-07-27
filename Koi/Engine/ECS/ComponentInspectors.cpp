#include "Koi/Engine/ECS/ComponentInspectors.h"
#include "Koi/Engine/Core/Core.h"

#if KOI_WITH_EDITOR

#include "Koi/Engine/Assets/MeshAsset.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <imgui.h>
#include <numbers>

namespace koi
{
	namespace
	{
		constexpr f32 kRadToDeg = 180.0f / static_cast<f32>(std::numbers::pi);
		constexpr f32 kDegToRad = static_cast<f32>(std::numbers::pi) / 180.0f;
	}

	void DrawTransformInspector(Entity e)
	{
		Transform* local = e.try_get_mut<Transform>();
		if (!local) return;

		// Euler angles are UI state: re-deriving them from the quaternion every frame makes
		// the drag widget snap near gimbal lock, so cache them until the selection changes
		// NOTE: This might need rework if I want multiple inspector panels
		static Vector3  s_eulerDeg = Vector3::Zero;
		static EntityID s_eulerFor = 0;

		if (s_eulerFor != e.id())
		{
			s_eulerDeg = local->Rotation.ToEuler() * kRadToDeg;
			s_eulerFor = e.id();
		}

		ImGui::DragFloat3("Position", &local->Position.x, 0.05f, ImGuiSliderFlags_ColorMarkers);
		if (ImGui::DragFloat3("Rotation", &s_eulerDeg.x, 0.5f))
		{
			local->Rotation = Quaternion::CreateFromYawPitchRoll(
				s_eulerDeg.y * kDegToRad, s_eulerDeg.x * kDegToRad, s_eulerDeg.z * kDegToRad);
		}
		ImGui::DragFloat3("Scale", &local->Scale.x, 0.05f, ImGuiSliderFlags_ColorMarkers);
	}

	void DrawMeshInspector(Entity e)
	{
		const MeshRef* meshRef = e.try_get<MeshRef>();
		if (!meshRef) return;

		const AssetCache<GPUMesh>& meshes = Engine::Get().GetAssets().Cache<GPUMesh>();

		ImGui::Separator();
		const std::string_view source = meshes.GetPath(meshRef->Handle);
		ImGui::TextWrapped("Source: %.*s", static_cast<int>(source.size()), source.data());

		if (const GPUMesh* mesh = meshes.Get(meshRef->Handle))
		{
			ImGui::Text("Draw calls  %d", static_cast<i32>(mesh->DrawItems.size()));
			ImGui::Text("Textures    %d", static_cast<i32>(mesh->Textures.size()));
		}
	}

	void DrawCameraInspector(Entity e)
	{
		CameraComponent* cam = e.try_get_mut<CameraComponent>();
		if (!cam) return;

		ImGui::Separator();
		ImGui::TextUnformatted("Camera");

		f32 fovDeg = cam->FovY * kRadToDeg;
		if (ImGui::DragFloat("FOV", &fovDeg, 0.5f, 1.0f, 179.0f))
			cam->FovY = fovDeg * kDegToRad;
		ImGui::DragFloat("Near", &cam->Near, 0.01f, 0.001f, cam->Far);
		ImGui::DragFloat("Far", &cam->Far, 1.0f, cam->Near, 100000.0f);
		ImGui::Checkbox("Primary", &cam->Primary);
	}

	void DrawLightInspector(Entity e)
	{
		Light* light = e.try_get_mut<Light>();
		if (!light) return;

		ImGui::Separator();
		ImGui::TextUnformatted("Light");

		const char* types[] = { "Directional", "Point" };
		int type = static_cast<int>(light->Type);
		if (ImGui::Combo("Type", &type, types, IM_ARRAYSIZE(types)))
			light->Type = static_cast<LightType>(type);

		ImGui::ColorEdit3("Color", &light->Color.x);
		ImGui::DragFloat("Intensity", &light->Intensity, 0.05f, 0.0f, 100.0f);
		if (light->Type == LightType::Point)
			ImGui::DragFloat("Range", &light->Range, 0.1f, 0.0f, 1000.0f);
	}
}

#else

namespace koi
{
	void DrawTransformInspector(Entity) {}
	void DrawMeshInspector(Entity)      {}
	void DrawCameraInspector(Entity)    {}
	void DrawLightInspector(Entity)     {}
}

#endif
