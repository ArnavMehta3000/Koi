#include "Koi/Editor/Inspectors/ComponentInspectors.h"
#include "Koi/Editor/Inspectors/ReflectInspector.h"
#include "Koi/Editor/UI/Widgets.h"
#include "Koi/Engine/Graphics/RenderPasses/AutoExposurePass.h"
#include "Koi/Engine/Assets/AssetRegistry.h"
#include "Koi/Engine/Assets/AssetTypes.h"
#include "Koi/Engine/Assets/MeshAsset.h"
#include "Koi/Engine/ECS/Components.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Math/Transform.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <imgui.h>
#include <algorithm>
#include <array>
#include <cmath>
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

		// Bins 1..N-1 as bars (bin 0 is the excluded black pixels), with a line at the average the exposure targets
		void DrawExposureHistogram(const AutoExposurePass::Stats& stats)
		{
			constexpr u32 kBars = HISTOGRAM_BIN_COUNT - 1;
			std::array<f32, kBars> bars{};
			f32 peak = 1.0f;
			for (u32 i = 0; i < kBars; ++i)
			{
				bars[i] = static_cast<f32>(stats.Bins[i + 1]);
				peak    = std::max(peak, bars[i]);
			}

			const ImVec2 size(ImGui::GetContentRegionAvail().x, 90.0f);
			ImGui::PlotHistogram("##histogram", bars.data(), static_cast<int>(kBars), 0, nullptr, 0.0f, peak, size);

			const AutoExposureSettings& s = stats.Settings;
			const f32 range   = std::max(s.MaxLogLuminance - s.MinLogLuminance, 0.01f);
			const f32 target  = s.KeyValue * std::exp2(s.ExposureCompensation);
			const f32 avgLum  = target / std::max(stats.Exposure, 1e-6f);
			const f32 t       = std::clamp((std::log2(avgLum) - s.MinLogLuminance) / range, 0.0f, 1.0f);

			const ImVec2 min = ImGui::GetItemRectMin();
			const ImVec2 max = ImGui::GetItemRectMax();
			const f32    x   = min.x + (t * (kBars - 1) + 0.5f) / kBars * (max.x - min.x);
			ImGui::GetWindowDrawList()->AddLine(ImVec2(x, min.y), ImVec2(x, max.y), ImGui::GetColorU32(ImGuiCol_PlotLinesHovered), 2.0f);
			ImGui::SetItemTooltip("Log luminance from %.1f to %.1f. The line is the frame's average", s.MinLogLuminance, s.MaxLogLuminance);

			ImGui::TextDisabled("Exposure x%.3f (%+.2f EV)   Avg luminance %.4f   Black pixels %u",
				stats.Exposure, std::log2(std::max(stats.Exposure, 1e-6f)), avgLum, stats.Bins[0]);
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
			ComponentOf<MeshRef>(e).Write(AssetKey(*dropped));
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
			ComponentOf<MaterialRef>(e).Write(AssetKey(*dropped));
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

	void PostProcessInspector::Draw(Entity e)
	{
		PostProcess* post = e.try_get_mut<PostProcess>();
		if (!post) return;

		ImGui::SeparatorText("Auto Exposure");
		if (!utils::BeginPropertyGrid("##autoexposure")) return;

		AutoExposureSettings& ae = post->AutoExposure;
		utils::PropertyLabel("Enabled", "Off leaves the image at exposure 1");
		ImGui::Checkbox("##Enabled", &ae.Enabled);

		ImGui::BeginDisabled(!ae.Enabled);
		utils::PropertyLabel("Min Log Luminance", "Darker pixels are left out of the average");
		ImGui::DragFloat("##MinLog", &ae.MinLogLuminance, 0.05f, -20.0f, ae.MaxLogLuminance - 0.1f, "%.2f EV");
		utils::PropertyLabel("Max Log Luminance", "Brighter pixels count as this bright");
		ImGui::DragFloat("##MaxLog", &ae.MaxLogLuminance, 0.05f, ae.MinLogLuminance + 0.1f, 20.0f, "%.2f EV");
		utils::PropertyLabel("Key Value", "The average luminance the frame is exposed to. 0.18 is middle grey");
		ImGui::DragFloat("##Key", &ae.KeyValue, 0.005f, 0.01f, 1.0f, "%.3f");
		utils::PropertyLabel("Compensation");
		ImGui::DragFloat("##Compensation", &ae.ExposureCompensation, 0.05f, -10.0f, 10.0f, "%+.2f EV");
		ImGui::EndDisabled();
		utils::EndPropertyGrid();

		if (ae.Enabled)
		{
			if (AutoExposurePass* pass = Engine::Get().GetSubsystem<Renderer>().GetFeatures().Find<AutoExposurePass>())
			{
				pass->RequestStats();
				if (auto stats = pass->GetStats(); stats.Valid)
				{
					DrawExposureHistogram(stats);
				}
				else
				{
					ImGui::TextDisabled("Waiting for the GPU...");
				}
			}
		}

		ImGui::SeparatorText("Tonemapping");
		if (!utils::BeginPropertyGrid("##tonemap")) return;

		utils::PropertyLabel("Operator", "None clamps, to judge exposure on its own");
		utils::DrawReflectedEnum("##Operator", post->Tonemap.Operator);

		utils::EndPropertyGrid();
	}
}
