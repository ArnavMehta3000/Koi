#pragma once
#include "Koi/Engine/Assets/AssetData.h"
#include "Koi/Engine/Assets/AssetHandles.h"
#include "Koi/Engine/Assets/AssetKey.h"
#include "Koi/Engine/Assets/AssetTraits.h"
#include "Koi/Engine/Graphics/Types.h"
#include <array>
#include <memory>
#include <string>

namespace koi
{
	class GfxDevice;
	struct MaterialTemplate;
	struct RasterPipeline;

	// The texture channels a material can bind, in the order the GPU row writes them
	enum class MaterialTextureSlot : u8
	{
		BaseColor = 0,
		MetallicRoughness,
		Normal,
		Occlusion,
		Emissive,
		COUNT
	};

	inline constexpr size_t kMaterialTextureCount = static_cast<size_t>(MaterialTextureSlot::COUNT);

	// What a .kmat decodes to, for today's fixed-slot Standard template
	struct MaterialSource
	{
		std::string Name;
		std::string Template;  // empty picks the library's default
		ImportMaterial::Constants Constants{};
		ImportMaterial::AlphaMode Mode = ImportMaterial::AlphaMode::Opaque;
		bool DoubleSided = false;
		std::array<AssetKey, kMaterialTextureCount> Textures{};
	};

	// A material with its textures resident. The DescriptorIndex is read back out of the texture cache when the GPU row is written, never stored here
	struct Material
	{
		Material() = default;
		Material(const Material&) = delete;
		Material& operator=(const Material&) = delete;
		Material(Material&&) noexcept = default;
		Material& operator=(Material&&) noexcept = default;

		std::string Name;
		ImportMaterial::Constants Constants{};
		ImportMaterial::AlphaMode Mode = ImportMaterial::AlphaMode::Opaque;
		bool DoubleSided = false;
		std::array<GPUTextureHandle, kMaterialTextureCount> Textures{};  // referenced, not owned

		u32 TableSlot = 0;  // the row this material owns in the renderer's MaterialTable

		const MaterialTemplate* Template = nullptr;  // owned by the MaterialTemplateLibrary
		const RasterPipeline*   Pipeline = nullptr;  // the template's variant for this cull mode

		// The key a slot's texture parameter is stored under
		[[nodiscard]] static std::string_view SlotName(MaterialTextureSlot slot);

		[[nodiscard]] DescriptorIndex SRVFor(MaterialTextureSlot slot) const;

		// Rewrites this material's table row from its current factors and textures
		void WriteRow() const;
		void Release(GfxDevice& device);
	};

	template<>
	struct AssetTraits<Material>
	{
		static constexpr std::string_view Name = "Material";

		using Source = MaterialSource;

		static std::unique_ptr<Source> Load(const std::string& vfsPath);
		static Material Upload(GfxDevice& device, const Source& source);
		static void Destroy(GfxDevice& device, Material& resource);
	};

}
