#include "Koi/Engine/ECS/ComponentInspectors.h"
#include "Koi/Engine/Core/Config.h"

#if KOI_WITH_EDITOR

#include "Koi/Engine/Assets/MeshAsset.h"
#include "Koi/Engine/Graphics/Procedural/MeshGenerators.h"
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
		static Vector3    s_eulerDeg = Vector3::Zero;
		static EntityID   s_eulerFor = 0;
		static Quaternion s_eulerRot = Quaternion::Identity;

		// Also re-derive when something else moved the entity (the viewport gizmo), or the widget would keep showing the angles from before the drag
		if (s_eulerFor != e.id() || s_eulerRot != local->Rotation)
		{
			s_eulerDeg = local->Rotation.ToEuler() * kRadToDeg;
			s_eulerFor = e.id();
			s_eulerRot = local->Rotation;
		}

		ImGui::DragFloat3("Position", &local->Position.x, 0.05f, ImGuiSliderFlags_ColorMarkers);
		if (ImGui::DragFloat3("Rotation", &s_eulerDeg.x, 0.5f))
		{
			local->Rotation = Quaternion::CreateFromYawPitchRoll(
				s_eulerDeg.y * kDegToRad, s_eulerDeg.x * kDegToRad, s_eulerDeg.z * kDegToRad);
			s_eulerRot = local->Rotation;
		}
		ImGui::DragFloat3("Scale", &local->Scale.x, 0.05f, ImGuiSliderFlags_ColorMarkers);
	}

	namespace
	{
		// One widget per params entry, dispatched on the node type
		bool DrawParam(const toml::key& key, toml::node& node, bool liveEdit)
		{
			const std::string label(key.str());

			if (toml::value<bool>* asBool = node.as_boolean())
				return ImGui::Checkbox(label.c_str(), &asBool->get());

			bool edited = false;

			if (toml::value<i64>* asInt = node.as_integer())
			{
				i32 value = static_cast<i32>(asInt->get());
				edited = ImGui::DragInt(label.c_str(), &value, 0.25f);
				if (edited) asInt->get() = value;
			}
			else if (toml::value<f64>* asFloat = node.as_floating_point())
			{
				f32 value = static_cast<f32>(asFloat->get());
				edited = ImGui::DragFloat(label.c_str(), &value, 0.01f);
				if (edited) asFloat->get() = ToTomlFloat(value);
			}
			else
			{
				// Vector2/3/4 arrive as arrays of floats
				toml::array* arr = node.as_array();
				if (!arr || arr->empty() || arr->size() > 4) return false;

				std::array<f32, 4> values{};
				for (size_t i = 0; i < arr->size(); ++i)
				{
					values[i] = static_cast<f32>((*arr)[i].value_or(0.0));
				}

				edited = ImGui::DragScalarN(label.c_str(), ImGuiDataType_Float, values.data(), static_cast<int>(arr->size()), 0.05f);

				if (edited)
				{
					toml::array replacement;
					for (size_t i = 0; i < arr->size(); ++i)
					{
						replacement.push_back(ToTomlFloat(values[i]));
					}

					*arr = std::move(replacement);
				}
			}

			return liveEdit ? edited : ImGui::IsItemDeactivatedAfterEdit();
		}

		void ReacquireMesh(Entity e, const std::string& uri)
		{
			GfxDevice& device = Engine::Get().GetSubsystem<Renderer>().GetDevice();
			AssetCache<GPUMesh>& meshes = Engine::Get().GetSubsystem<AssetRegistry>().Cache<GPUMesh>();

			const MeshHandle handle = meshes.Acquire(device, uri);
			if (!handle) return;

			if (const MeshRef* previous = e.try_get<MeshRef>())
				meshes.Release(device, previous->Handle);

			e.set<MeshRef>({ handle });
		}

		void DrawProceduralInspector(Entity e, std::string_view source)
		{
			// The edited params are UI state: a drag only commits on release
			static EntityID    s_owner = 0;
			static std::string s_committed;
			static std::string s_generator;
			static toml::table s_params;

			if (s_owner != e.id() || s_committed != source)
			{
				std::optional parsed = ParseProceduralUri(source);
				if (!parsed) return;

				s_generator = std::move(parsed->first);
				s_params    = std::move(parsed->second);
				s_owner     = e.id();
				s_committed = source;
			}

			// Off by default because every committed edit regenerates the mesh and re-uploads it
			static bool s_liveUpdate = false;

			bool changed = false;

			if (ImGui::BeginCombo("Generator", s_generator.c_str()))
			{
				for (const MeshGeneratorInfo& info : MeshGeneratorRegistry::Get().All())
				{
					if (!ImGui::Selectable(info.Name.c_str(), info.Name == s_generator)) continue;

					// A different generator takes a different params set, so start from its defaults
					s_generator = info.Name;
					s_params    = info.Defaults();
					changed     = true;
				}

				ImGui::EndCombo();
			}

			ImGui::Checkbox("Live update", &s_liveUpdate);
			ImGui::SetItemTooltip("Rebuild and re-upload the mesh on every frame of a drag, "
			                      "instead of once when the drag ends");

			for (auto&& [key, node] : s_params)
			{
				changed |= DrawParam(key, node, s_liveUpdate);
			}

			// Clamp the params to the generator's range, so the widget cannot be dragged out of bounds
			if (const MeshGeneratorInfo* info = MeshGeneratorRegistry::Get().Find(s_generator))
			{
				if (info->Clamp) info->Clamp(s_params);
			}

			if (!changed) return;

			s_committed = ToProceduralUri(s_generator, s_params);
			ReacquireMesh(e, s_committed);
		}
	}

	void DrawMeshInspector(Entity e)
	{
		const MeshRef* meshRef = e.try_get<MeshRef>();
		if (!meshRef) return;

		const AssetCache<GPUMesh>& meshes = Engine::Get().GetSubsystem<AssetRegistry>().Cache<GPUMesh>();

		ImGui::Separator();
		const std::string_view source = meshes.GetPath(meshRef->Handle);
		ImGui::TextWrapped("Source: %.*s", static_cast<int>(source.size()), source.data());

		if (const GPUMesh* mesh = meshes.Get(meshRef->Handle))
		{
			ImGui::Text("Draw calls  %d", static_cast<i32>(mesh->DrawItems.size()));
			ImGui::Text("Textures    %d", static_cast<i32>(mesh->Textures.size()));
		}

		if (source.starts_with(kProceduralScheme))
			DrawProceduralInspector(e, source);
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

		if (light->Type == LightType::Directional)
		{
			ImGui::Checkbox("Casts Shadows", &light->CastsShadows);
			if (light->CastsShadows)
				ImGui::DragFloat("Shadow Bias", &light->ShadowBias, 0.0001f, 0.0f, 0.05f, "%.4f");
		}
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
