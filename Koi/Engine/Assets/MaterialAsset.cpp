#include "Koi/Engine/Assets/MaterialAsset.h"
#include "Koi/Engine/Assets/MaterialFile.h"
#include "Koi/Engine/Assets/AssetRegistry.h"
#include "Koi/Engine/Graphics/GPUTexture.h"
#include "Koi/Engine/Graphics/MaterialTemplateLibrary.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Runtime/Engine.h"

namespace koi
{
	static_assert(AssetType<Material>, "AssetTraits<Material> does not satisfy AssetType");

	namespace
	{
		// The [textures] key each slot is written under
		constexpr std::array<std::string_view, kMaterialTextureCount> kSlotKeys =
		{
			"base_color", "metallic_roughness", "normal", "occlusion", "emissive"
		};
	}

	std::string_view Material::SlotName(MaterialTextureSlot slot)
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
		if (AssetFile::TypeForPath(vfsPath) != AssetFileType::Material)
		{
			KOI_LOG(Assets, Error, "{} isn't a .kmat, and materials can only be loaded from those", vfsPath);
			return nullptr;
		}

		const std::optional<AssetFile> file = AssetFile::Load(vfsPath);
		if (!file) return nullptr;

		const std::optional<MaterialFileContents> contents = MaterialFileContents::Read(*file, vfsPath);
		if (!contents) return nullptr;

		const AssetIndex& index = Engine::Get().GetSubsystem<AssetRegistry>().Index();
		return std::make_unique<MaterialSource>(contents->ToSource([&](const UUID& id)
		{
			if (id.is_nil()) return AssetKey();
			if (const AssetIndexEntry* entry = index.Find(id)) return AssetKey(entry->VirtualPath);

			KOI_LOG(Assets, Warn, "{} uses texture {}, which isn't in the project anymore", vfsPath, utils::ToString(id));
			return AssetKey();
		}));
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
			{
				KOI_LOG(Assets, Warn, "Material '{}' names unknown template '{}'", source.Name, source.Template);
			}

			material.Template = templates.Default();
		}

		if (material.Template)
			material.Pipeline = &material.Template->PipelineFor(material.DoubleSided);

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
