#include "Koi/Editor/Inspectors/ComponentInspectors.h"
#include "Koi/Editor/UI/Widgets.h"
#include "Koi/Engine/Assets/AssetRegistry.h"
#include "Koi/Engine/Assets/AssetTypes.h"
#include "Koi/Engine/Assets/MeshAsset.h"
#include "Koi/Engine/ECS/Components.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Math/Transform.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <imgui.h>
#include <filesystem>
#include <numbers>
#include <optional>

namespace koi::editor
{
	namespace
	{
		constexpr f32 kRadToDeg = 180.0f / static_cast<f32>(std::numbers::pi);
		constexpr f32 kDegToRad = static_cast<f32>(std::numbers::pi) / 180.0f;

		AssetCache<GPUMesh>&  Meshes()    { return Engine::Get().GetSubsystem<AssetRegistry>().Cache<GPUMesh>(); }
		AssetCache<Material>& Materials() { return Engine::Get().GetSubsystem<AssetRegistry>().Cache<Material>(); }
		GfxDevice&            Device()    { return Engine::Get().GetSubsystem<Renderer>().GetDevice(); }

		// Acquire before remove: if the new key is the one already held, the extra reference
		// keeps the asset resident across the removal that releases the old handle
		template<typename Ref, typename T>
		static void Reacquire(Entity e, AssetCache<T>& cache, const AssetKey& key)
		{
			const AssetHandle<T> handle = cache.Acquire(Device(), key);
			if (!handle) return;

			e.remove<Ref>();  // World's on_remove hook releases whatever was there
			e.set<Ref>({ handle });
		}

		// A property row holding an asset slot; returns the dropped path when it is the right kind
		static std::optional<std::string> AssetRow(const char* label, std::string_view current, AssetKind kind)
		{
			utils::PropertyLabel(label);

			const std::string name = std::filesystem::path(current).stem().string();
			std::string dropped;
			if (!utils::AssetField(label, utils::AssetKindIcon(kind), name, current, dropped)) return std::nullopt;

			if (const AssetTypeInfo* type = koi::utils::FindAssetTypeForFile(dropped); type && type->Kind == kind)
				return dropped;

			KOI_LOG(Editor, Warn, "Dropped asset is not a {}: {}", koi::utils::FindAssetTypeByKind(kind)->Name, dropped);
			return std::nullopt;
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

		if (!utils::BeginPropertyGrid("##transform")) return;

		utils::PropertyLabel("Position");
		utils::DragAxes("##Position", &local->Position.x, 3, 0.05f);

		utils::PropertyLabel("Rotation", "Euler angles in degrees");
		if (utils::DragAxes("##Rotation", &m_eulerDeg.x, 3, 0.5f, "%.1f"))
		{
			local->Rotation = Quaternion::CreateFromYawPitchRoll(
				m_eulerDeg.y * kDegToRad, m_eulerDeg.x * kDegToRad, m_eulerDeg.z * kDegToRad);
			m_eulerRot = local->Rotation;
		}

		utils::PropertyLabel("Scale");
		utils::DragAxes("##Scale", &local->Scale.x, 3, 0.05f);

		utils::EndPropertyGrid();
	}

	void MeshInspector::Draw(Entity e)
	{
		const MeshRef* meshRef = e.try_get<MeshRef>();
		if (!meshRef) return;

		if (!utils::BeginPropertyGrid("##mesh")) return;

		// The old handle was just released, so meshRef is stale - resume next frame
		if (const auto dropped = AssetRow("Mesh", Meshes().GetPath(meshRef->Handle), AssetKind::Mesh))
		{
			Reacquire<MeshRef>(e, Meshes(), AssetKey(*dropped));
			utils::EndPropertyGrid();
			return;
		}

		if (const GPUMesh* mesh = Meshes().Get(meshRef->Handle))
		{
			utils::PropertyLabel("Draw Calls");
			ImGui::TextDisabled("%d", static_cast<i32>(mesh->DrawItems.size()));
			utils::PropertyLabel("Materials");
			ImGui::TextDisabled("%d", static_cast<i32>(mesh->Materials.size()));
		}
		utils::EndPropertyGrid();
	}

	void MaterialRefInspector::Draw(Entity e)
	{
		const MaterialRef* ref = e.try_get<MaterialRef>();
		if (!ref) return;

		if (!utils::BeginPropertyGrid("##material")) return;

		const auto dropped = AssetRow("Material", Materials().GetPath(ref->Handle), AssetKind::Material);
		utils::EndPropertyGrid();

		if (dropped)
		{
			Reacquire<MaterialRef>(e, Materials(), AssetKey(*dropped));
			return;
		}

		ImGui::TextDisabled("Select the .kmat in the Assets panel to edit it");
	}

	void CameraInspector::Draw(Entity e)
	{
		CameraComponent* cam = e.try_get_mut<CameraComponent>();
		if (!cam) return;

		if (!utils::BeginPropertyGrid("##camera")) return;

		f32 fovDeg = cam->FovY * kRadToDeg;
		utils::PropertyLabel("Field of View", "Vertical, in degrees");
		if (ImGui::DragFloat("##FOV", &fovDeg, 0.5f, 1.0f, 179.0f, "%.1f"))
		{
			cam->FovY = fovDeg * kDegToRad;
		}

		utils::PropertyLabel("Near");
		ImGui::DragFloat("##Near", &cam->Near, 0.01f, 0.001f, cam->Far);
		utils::PropertyLabel("Far");
		ImGui::DragFloat("##Far", &cam->Far, 1.0f, cam->Near, 100000.0f);
		utils::PropertyLabel("Primary", "The camera the game renders from");
		ImGui::Checkbox("##Primary", &cam->Primary);

		utils::EndPropertyGrid();
	}

	void LightInspector::Draw(Entity e)
	{
		Light* light = e.try_get_mut<Light>();
		if (!light) return;

		if (!utils::BeginPropertyGrid("##light")) return;

		const char* types[] = { "Directional", "Point" };
		int type = static_cast<int>(light->Type);
		utils::PropertyLabel("Type");
		if (ImGui::Combo("##Type", &type, types, IM_ARRAYSIZE(types)))
		{
			light->Type = static_cast<LightType>(type);
		}

		utils::PropertyLabel("Color");
		ImGui::ColorEdit3("##Color", &light->Color.x);
		utils::PropertyLabel("Intensity");
		ImGui::DragFloat("##Intensity", &light->Intensity, 0.05f, 0.0f, 100.0f);
		if (light->Type == LightType::Point)
		{
			utils::PropertyLabel("Range");
			ImGui::DragFloat("##Range", &light->Range, 0.1f, 0.0f, 1000.0f);
		}

		if (light->Type == LightType::Directional)
		{
			utils::PropertyLabel("Casts Shadows");
			ImGui::Checkbox("##CastsShadows", &light->CastsShadows);
			if (light->CastsShadows)
			{
				utils::PropertyLabel("Shadow Bias");
				ImGui::DragFloat("##ShadowBias", &light->ShadowBias, 0.0001f, 0.0f, 0.05f, "%.4f");
				utils::PropertyLabel("Shadow Extent", "Half-size of the shadow box around this entity. 0 fits the whole scene");
				ImGui::DragFloat("##ShadowExtent", &light->ShadowExtent, 0.5f, 0.0f, 10000.0f, "%.1f");
			}
		}

		utils::EndPropertyGrid();
	}
}
