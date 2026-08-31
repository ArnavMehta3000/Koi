#include "Koi/Editor/Inspectors/ComponentInspectors.h"
#include "Koi/Editor/Inspectors/InspectorRegistry.h"
#include "Koi/Engine/Core/Patterns/Match.h"
#include "Koi/Engine/Assets/AssetTypes.h"
#include "Koi/Engine/Assets/MeshAsset.h"
#include "Koi/Engine/ECS/Components.h"
#include "Koi/Engine/Graphics/Procedural/MeshGenerators.h"
#include "Koi/Engine/Graphics/Procedural/ProceduralMeshSpec.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Interop/EditorContext.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Math/Transform.h"
#include "Koi/Engine/Runtime/AppBase.h"
#include "Koi/Engine/Runtime/Engine.h"
#include "Koi/Engine/Scripting/ScriptComponent.h"
#include <imgui.h>
#include <imgui_stdlib.h>
#include <array>
#include <numbers>
#include <type_traits>

namespace koi::editor
{
	namespace
	{
		constexpr f32 kRadToDeg = 180.0f / static_cast<f32>(std::numbers::pi);
		constexpr f32 kDegToRad = static_cast<f32>(std::numbers::pi) / 180.0f;

		AssetCache<GPUMesh>& Meshes() { return Engine::Get().GetSubsystem<AssetRegistry>().Cache<GPUMesh>(); }
		GfxDevice&           Device() { return Engine::Get().GetSubsystem<Renderer>().GetDevice(); }

		// Acquire before remove: if the new uri is the one already held, the extra reference
		// keeps the mesh resident across the removal that releases the old handle
		static void ReacquireMesh(Entity e, const std::string& uri)
		{
			const MeshHandle handle = Meshes().Acquire(Device(), uri);
			if (!handle) return;

			e.remove<MeshRef>();  // World's on_remove hook releases whatever was there
			e.set<MeshRef>({ handle });
		}

		// One widget per params entry, dispatched on the node type
		static bool DrawParam(const toml::key& key, toml::node& node, bool liveEdit)
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

				// The panel drags every asset kind, so reject anything that is not a mesh
				const size_t dot = dropped.find_last_of('.');
				const AssetTypeInfo* type = dot == std::string::npos
					? nullptr
					: FindAssetType(dropped.substr(dot));

				if (type && type->Kind == AssetKind::Mesh)
				{
					ReacquireMesh(e, dropped);
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
			ImGui::Text("Textures    %d", static_cast<i32>(mesh->Textures.size()));
		}

		if (source.starts_with(kProceduralScheme))
			DrawProcedural(e, source);
	}

	void MeshInspector::DrawProcedural(Entity e, std::string_view source)
	{
		if (m_owner != e.id() || m_committed != source)
		{
			std::optional parsed = ParseProceduralUri(source);
			if (!parsed) return;

			m_generator = std::move(parsed->first);
			m_params    = std::move(parsed->second);
			m_owner     = e.id();
			m_committed = source;
		}

		bool changed = false;

		if (ImGui::BeginCombo("Generator", m_generator.c_str()))
		{
			for (const MeshGeneratorInfo& info : MeshGeneratorRegistry::Get().All())
			{
				if (!ImGui::Selectable(info.Name.c_str(), info.Name == m_generator)) continue;

				// A different generator takes a different params set, so start from its defaults
				m_generator = info.Name;
				m_params    = info.Defaults();
				changed     = true;
			}

			ImGui::EndCombo();
		}

		// Off by default because every committed edit regenerates the mesh and re-uploads it
		ImGui::Checkbox("Live update", &m_liveUpdate);
		ImGui::SetItemTooltip("Rebuild and re-upload the mesh on every frame of a drag, "
		                      "instead of once when the drag ends");

		for (auto&& [key, node] : m_params)
		{
			changed |= DrawParam(key, node, m_liveUpdate);
		}

		// Clamp the params to the generator's range, so the widget cannot be dragged out of bounds
		if (const MeshGeneratorInfo* info = MeshGeneratorRegistry::Get().Find(m_generator))
		{
			if (info->Clamp) info->Clamp(m_params);
		}

		if (!changed) return;

		m_committed = ToProceduralUri(m_generator, m_params);
		ReacquireMesh(e, m_committed);
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

	void ScriptInspector::Draw(Entity e)
	{
		ScriptComponent* sc = e.try_get_mut<ScriptComponent>();
		if (!sc) return;

		ImGui::InputText("Path", &sc->ScriptPath);

		ImGui::BeginDisabled(sc->ScriptPath.empty());
		if (ImGui::SmallButton("Edit"))
		{
			if (IEditorContext* editor = AppBase::Get().GetEditorContext())
			{
				editor->OpenScriptEditor(sc->ScriptPath);
			}
		}
		ImGui::EndDisabled();

		// Stored and editable, but not yet pushed into the live script instance's fields -
		// that needs a metadata contract for which fields are script-exposed (a later milestone)
		for (ScriptProperty& prop : sc->Properties)
		{
			std::visit(Match
				{
					[&](bool& value)        { ImGui::Checkbox(prop.Name.c_str(), &value);         },
					[&](i32& value)         { ImGui::DragInt(prop.Name.c_str(), &value);          },
					[&](f32& value)         { ImGui::DragFloat(prop.Name.c_str(), &value, 0.01f); },
					[&](std::string& value) { ImGui::InputText(prop.Name.c_str(), &value);        },
				}, prop.Value);
		}
	}

	void RegisterBuiltinInspectors(InspectorRegistry& registry)
	{
		registry.Emplace<TransformInspector>();
		registry.Emplace<MeshInspector>();
		registry.Emplace<CameraInspector>();
		registry.Emplace<LightInspector>();
		registry.Emplace<ScriptInspector>();
	}
}
