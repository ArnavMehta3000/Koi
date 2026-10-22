#include "Koi/Editor/Inspectors/MaterialInspector.h"
#include "Koi/Editor/Assets/AssetDatabase.h"
#include "Koi/Editor/Assets/MaterialWriter.h"
#include "Koi/Editor/UI/Widgets.h"
#include "Koi/Engine/Assets/AssetRegistry.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <IconsFontAwesome6.h>
#include <imgui.h>
#include <cctype>

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

		// "base_color" -> "Base Color"
		std::string DisplayName(std::string_view name)
		{
			std::string out;
			bool upper = true;
			for (const char c : name)
			{
				if (c == '_')
				{
					out += ' ';
					upper = true;
					continue;
				}
				out += upper ? static_cast<char>(std::toupper(static_cast<unsigned char>(c))) : c;
				upper = false;
			}
			return out;
		}

		bool DrawFloats(const MaterialParam& param, ImportMaterial::Constants& constants)
		{
			f32* field = FloatFieldFor(param.Name, constants);
			if (!field) return false;

			utils::PropertyLabel(DisplayName(param.Name).c_str());
			const std::string id = "##" + std::string(param.Name);
			switch (param.Type)
			{
			case MaterialParamType::Float:  return ImGui::DragFloat (id.c_str(), field, 0.01f, 0.0f, 1.0f);
			case MaterialParamType::Float3: return ImGui::ColorEdit3(id.c_str(), field);
			case MaterialParamType::Float4: return ImGui::ColorEdit4(id.c_str(), field);
			default:                        return false;
			}
		}
	}

	void MaterialInspector::Draw(AssetDatabase& database, const AssetEntry& entry)
	{
		if (entry.VirtualPath != m_loaded) Load(entry);

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

		if (utils::BeginPropertyGrid("##material"))
		{
			utils::PropertyLabel("Template");
			ImGui::TextDisabled("%s", target->Name.c_str());

			utils::PropertyLabel("Double Sided");
			if (ImGui::Checkbox("##double_sided", &m_source.DoubleSided)) m_dirty = true;

			for (const MaterialParam& param : target->Parameters)
			{
				if (param.Type == MaterialParamType::Texture) DrawTextureSlot(database, param);
				else if (DrawFloats(param, m_source.Constants)) m_dirty = true;
			}
			utils::EndPropertyGrid();
		}

		ImGui::Spacing();
		ImGui::Separator();

		ImGui::BeginDisabled(!m_dirty);
		if (ImGui::Button(ICON_FA_FLOPPY_DISK "  Save", ImVec2(120.0f, 0.0f))) Save(database, entry);
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
			if (Material::SlotName(static_cast<MaterialTextureSlot>(i)) == param.Name) slot = i;
		}
		if (slot == kMaterialTextureCount) return;

		AssetKey& key = m_source.Textures[slot];

		const AssetEntry* bound = key.IsValid() ? database.Find(key.Path()) : nullptr;
		const std::string name  = bound ? bound->Name : (key.IsValid() ? key.Path() : std::string());
		const std::string id(param.Name);

		utils::PropertyLabel(DisplayName(param.Name).c_str());
		std::string dropped;
		if (!utils::AssetField(id.c_str(), utils::AssetKindIcon(AssetKind::Texture), name, key.IsValid() ? key.Path() : std::string_view(), dropped, true)) return;

		if (dropped.empty())
		{
			key     = AssetKey();
			m_dirty = true;
		}
		else if (const AssetEntry* dropEntry = database.Find(dropped); dropEntry && dropEntry->Kind == AssetKind::Texture)
		{
			key     = dropEntry->Key();
			m_dirty = true;
		}
	}

	void MaterialInspector::Load(const AssetEntry& entry)
	{
		m_loaded = entry.VirtualPath;
		m_dirty  = false;

		if (const Result<std::unique_ptr<MaterialSource>> source = AssetTraits<Material>::Load(entry.VirtualPath))
		{
			m_source = std::move(**source);
		}
		else
		{
			KOI_LOG(AssetDB, Error, "Couldn't open {}: {}", entry.VirtualPath, source.error().Message);
			m_source = MaterialSource{};
		}
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

		if (material->Template)
			material->Pipeline = &material->Template->PipelineFor(material->DoubleSided);

		material->WriteRow();
	}
}
