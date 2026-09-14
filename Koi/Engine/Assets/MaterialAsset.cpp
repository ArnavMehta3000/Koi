#include "Koi/Engine/Assets/MaterialAsset.h"
#include "Koi/Engine/Assets/AssetRegistry.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/IO/VirtualFileSystem.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <toml++/toml.hpp>

namespace koi
{
	namespace
	{
		// The [textures] key each slot is written under
		constexpr std::array<std::string_view, kMaterialTextureCount> kSlotKeys =
		{
			"base_color", "metallic_roughness", "normal", "occlusion", "emissive"
		};

		static ImportMaterial::AlphaMode ParseAlphaMode(std::string_view text)
		{
			if (text == "mask")  return ImportMaterial::AlphaMode::Mask;
			if (text == "blend") return ImportMaterial::AlphaMode::Blend;
			return ImportMaterial::AlphaMode::Opaque;
		}

		template<typename View, typename T>
		void ReadFloats(View node, T& out, size_t count)
		{
			const toml::array* array = node.as_array();
			if (!array) return;

			f32* fields = &out.x;
			for (size_t i = 0; i < count && i < array->size(); ++i)
			{
				fields[i] = static_cast<f32>((*array)[i].value_or(fields[i]));
			}
		}
	}

	std::string_view ToString(MaterialTextureSlot slot)
	{
		const size_t index = static_cast<size_t>(slot);
		return index < kSlotKeys.size() ? kSlotKeys[index] : std::string_view("unknown");
	}

	DescriptorIndex Material::SRVFor(MaterialTextureSlot slot) const
	{
		const size_t index = static_cast<size_t>(slot);
		if (index >= Textures.size()) return INVALID_DESCRIPTOR;

		const GPUTexture* texture = Engine::Get().GetSubsystem<AssetRegistry>().Cache<GPUTexture>().Get(Textures[index]);
		return texture ? texture->SRVIndex : INVALID_DESCRIPTOR;
	}

	void Material::WriteRow() const
	{
		Renderer& renderer = Engine::Get().GetSubsystem<Renderer>();

		// Albedo is the one channel the shader always samples, so it never stays unbound
		const DescriptorIndex albedoSRV = SRVFor(MaterialTextureSlot::BaseColor);

		renderer.GetMaterialTable().Set(TableSlot, GPUMaterialData
		{
			.BaseColorFactor        = Constants.BaseColorFactor,
			.EmissiveFactor         = Vector4(Constants.EmissiveFactor.x, Constants.EmissiveFactor.y, Constants.EmissiveFactor.z, 0.0f),
			.MetallicFactor         = Constants.MetallicFactor,
			.RoughnessFactor        = Constants.RoughnessFactor,
			.NormalScale            = Constants.NormalScale,
			.OcclusionStrength      = Constants.OcclusionStrength,
			.AlphaCutoff            = Constants.AlphaCutoff,
			.BaseColorIndex         = albedoSRV != INVALID_DESCRIPTOR ? albedoSRV : renderer.GetDefaultResources().WhiteTextureSRV,
			.MetallicRoughnessIndex = SRVFor(MaterialTextureSlot::MetallicRoughness),
			.NormalIndex            = SRVFor(MaterialTextureSlot::Normal),
			.OcclusionIndex         = SRVFor(MaterialTextureSlot::Occlusion),
			.EmissiveIndex          = SRVFor(MaterialTextureSlot::Emissive),
		});
	}

	void Material::Release(GfxDevice& device)
	{
		Engine::Get().GetSubsystem<Renderer>().GetMaterialTable().Free(TableSlot);
		TableSlot = MaterialTable::kFallbackSlot;

		AssetCache<GPUTexture>& textures = Engine::Get().GetSubsystem<AssetRegistry>().Cache<GPUTexture>();
		for (GPUTextureHandle& handle : Textures)
		{
			textures.Release(device, handle);
			handle = {};
		}
	}

	std::unique_ptr<MaterialSource> AssetTraits<Material>::Load(const std::string& vfsPath)
	{
		const fs::path diskPath = VirtualFileSystem::Get().TryResolveToDisk(vfsPath).value_or(fs::path(vfsPath));

		toml::table root;
		try
		{
			root = toml::parse_file(diskPath.string());
		}
		catch (const toml::parse_error& e)
		{
			KOI_LOG_ERROR("Material '{}' did not parse: {}", diskPath.string(), e.description());
			return nullptr;
		}

		auto source = std::make_unique<MaterialSource>();
		source->Name        = root["name"].value_or(diskPath.stem().string());
		source->Template    = root["template"].value_or(std::string());
		source->Mode        = ParseAlphaMode(root["alpha_mode"].value_or(std::string_view("opaque")));
		source->DoubleSided = root["double_sided"].value_or(false);

		ImportMaterial::Constants& c = source->Constants;
		const auto constants = root["constants"];
		ReadFloats(constants["base_color"], c.BaseColorFactor, 4);
		ReadFloats(constants["emissive"],   c.EmissiveFactor,  3);
		c.MetallicFactor    = constants["metallic"].value_or(c.MetallicFactor);
		c.RoughnessFactor   = constants["roughness"].value_or(c.RoughnessFactor);
		c.NormalScale       = constants["normal_scale"].value_or(c.NormalScale);
		c.OcclusionStrength = constants["occlusion_strength"].value_or(c.OcclusionStrength);
		c.AlphaCutoff       = constants["alpha_cutoff"].value_or(c.AlphaCutoff);

		const auto textures = root["textures"];
		for (size_t i = 0; i < kMaterialTextureCount; ++i)
		{
			if (const std::optional<std::string> key = textures[kSlotKeys[i]].value<std::string>())
				source->Textures[i] = AssetKey(*key);
		}

		return source;
	}

	Material AssetTraits<Material>::Upload(GfxDevice& device, const MaterialSource& source)
	{
		Material material;
		material.Name        = source.Name;
		material.Constants   = source.Constants;
		material.Mode        = source.Mode;
		material.DoubleSided = source.DoubleSided;

		AssetCache<GPUTexture>& textures = Engine::Get().GetSubsystem<AssetRegistry>().Cache<GPUTexture>();
		for (size_t i = 0; i < kMaterialTextureCount; ++i)
		{
			// An unbound slot stays empty, the GPU row falls back when it reads no SRV
			if (source.Textures[i].IsValid())
				material.Textures[i] = textures.Acquire(device, source.Textures[i]);
		}

		Renderer& renderer = Engine::Get().GetSubsystem<Renderer>();

		MaterialTemplateLibrary& templates = renderer.GetMaterialTemplates();
		material.Template = source.Template.empty() ? templates.Default() : templates.Find(source.Template);
		if (!material.Template)
		{
			if (!source.Template.empty())
				KOI_LOG_WARN("Material '{}' names unknown template '{}'", source.Name, source.Template);

			material.Template = templates.Default();
		}

		if (material.Template) material.Pipeline = material.Template->PipelineFor(material.DoubleSided);

		// The row is only writable once the textures are resident, since it stores their indices
		material.TableSlot = renderer.GetMaterialTable().Allocate({});
		material.WriteRow();

		return material;
	}

	void AssetTraits<Material>::Destroy(GfxDevice& device, Material& resource)
	{
		resource.Release(device);
	}
}
