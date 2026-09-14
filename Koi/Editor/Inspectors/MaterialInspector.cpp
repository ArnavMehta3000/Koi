#include "Koi/Editor/Inspectors/MaterialInspector.h"
#include "Koi/Editor/Assets/AssetDatabase.h"
#include "Koi/Editor/Assets/MaterialFile.h"
#include "Koi/Engine/Assets/AssetRegistry.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <imgui.h>

namespace koi::editor
{
	namespace
	{
		// Where each declared parameter reads and writes. The template says which of these to
		// show; this says what each one is
		f32* FloatFieldFor(std::string_view name, ImportMaterial::Constants& c)
		{
			if (name == "base_color")         return &c.BaseColorFactor.x;
			if (name == "emissive")           return &c.EmissiveFactor.x;
			if (name == "metallic")           return &c.MetallicFactor;
			if (name == "roughness")          return &c.RoughnessFactor;
			if (name == "normal_scale")       return &c.NormalScale;
			if (name == "occlusion_strength") return &c.OcclusionStrength;
			if (name == "alpha_cutoff")       return &c.AlphaCutoff;
			return nullptr;
		}

		bool DrawFloats(const MaterialParam& param, ImportMaterial::Constants& constants)
		{
			f32* field = FloatFieldFor(param.Name, constants);
			if (!field) return false;

			const std::string label(param.Name);
			switch (param.Type)
			{
			case MaterialParamType::Float:  return ImGui::DragFloat (label.c_str(), field, 0.01f, 0.0f, 1.0f);
			case MaterialParamType::Float3: return ImGui::ColorEdit3(label.c_str(), field);
			case MaterialParamType::Float4: return ImGui::ColorEdit4(label.c_str(), field);
			default:                        return false;
			}
		}
	}

	void MaterialInspector::Draw(AssetDatabase& database, const AssetEntry& entry)
	{
		if (entry.Id != m_loaded) Load(entry);

		ImGui::TextDisabled("%s", entry.VirtualPath.c_str());
		ImGui::Separator();
		ImGui::Spacing();

		MaterialTemplateLibrary& templates = Engine::Get().GetSubsystem<Renderer>().GetMaterialTemplates();
		const MaterialTemplate*  target    = m_source.Template.empty() ? templates.Default() : templates.Find(m_source.Template);
		if (!target)
		{
			ImGui::TextDisabled("No material template registered");
			return;
		}

		ImGui::LabelText("template", "%s", target->Name.c_str());

		if (ImGui::Checkbox("double_sided", &m_source.DoubleSided)) m_dirty = true;

		ImGui::Spacing();

		for (const MaterialParam& param : target->Parameters)
		{
			if (param.Type == MaterialParamType::Texture) DrawTextureSlot(database, param);
			else if (DrawFloats(param, m_source.Constants)) m_dirty = true;
		}

		ImGui::Spacing();
		ImGui::Separator();

		ImGui::BeginDisabled(!m_dirty);
		if (ImGui::Button("Save", ImVec2(120.0f, 0.0f))) Save(database, entry);
		ImGui::EndDisabled();

		if (m_dirty)
		{
			ImGui::SameLine();
			ImGui::TextDisabled("unsaved");
		}
	}

	// A texture parameter's name is its slot's name, which is also its key in the [textures] table
	void MaterialInspector::DrawTextureSlot(AssetDatabase& database, const MaterialParam& param)
	{
		size_t slot = kMaterialTextureCount;
		for (size_t i = 0; i < kMaterialTextureCount; ++i)
		{
			if (ToString(static_cast<MaterialTextureSlot>(i)) == param.Name) slot = i;
		}
		if (slot == kMaterialTextureCount) return;

		AssetKey& key = m_source.Textures[slot];

		const AssetEntry* bound = key.IsValid() && !key.IsUri() ? database.Find(key.Id()) : nullptr;
		const std::string label = bound ? bound->Name : (key.IsValid() ? key.ToString() : std::string("(none)"));

		ImGui::PushID(static_cast<int>(slot));
		ImGui::Button(label.c_str(), ImVec2(-70.0f, 0.0f));

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kAssetDragDropType))
			{
				const std::string dropped(static_cast<const char*>(payload->Data));
				if (const AssetEntry* dropEntry = database.Find(dropped); dropEntry && dropEntry->Kind == AssetKind::Texture)
				{
					key     = dropEntry->Key();
					m_dirty = true;
				}
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::SameLine();
		if (ImGui::SmallButton("x"))
		{
			key     = AssetKey();
			m_dirty = true;
		}

		ImGui::SameLine();
		ImGui::TextUnformatted(param.Name.data(), param.Name.data() + param.Name.size());
		ImGui::PopID();
	}

	void MaterialInspector::Load(const AssetEntry& entry)
	{
		m_loaded = entry.Id;
		m_dirty  = false;

		if (const std::unique_ptr<MaterialSource> source = AssetTraits<Material>::Load(entry.VirtualPath))
			m_source = std::move(*source);
		else
			m_source = MaterialSource{};
	}

	void MaterialInspector::Save(AssetDatabase& database, const AssetEntry& entry)
	{
		if (!WriteMaterial(entry.DiskPath, m_source)) return;

		m_dirty = false;
		ApplyToResident(entry);
		database.Refresh();
	}

	void MaterialInspector::ApplyToResident(const AssetEntry& entry) const
	{
		AssetCache<Material>& cache = Engine::Get().GetSubsystem<AssetRegistry>().Cache<Material>();

		const AssetKey key = entry.Key();
		if (!cache.IsResident(key)) return;

		// Editing the resident copy avoids a reload: the row is rebuilt, the textures are untouched
		Material* material = cache.Find(key);
		if (!material) return;

		material->Constants   = m_source.Constants;
		material->DoubleSided = m_source.DoubleSided;
		if (material->Template) material->Pipeline = material->Template->PipelineFor(material->DoubleSided);

		material->WriteRow();
	}
}
