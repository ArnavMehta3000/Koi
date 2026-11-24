#pragma once
#include "Koi/Engine/Assets/AssetHandles.h"
#include "Koi/Engine/Assets/AssetKey.h"
#include "Koi/Engine/Assets/AssetTraits.h"
#include "Koi/Engine/Assets/MaterialSource.h"
#include "Koi/Engine/RHI/Types.h"
#include <array>
#include <memory>
#include <string>

namespace koi
{
	class GfxDevice;
	struct MaterialTemplate;
	struct RasterPipeline;

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
		const RasterPipeline*   Pipeline = nullptr;  // the template's variant for Mode and DoubleSided

		[[nodiscard]] DescriptorIndex SRVFor(MaterialTextureSlot slot) const;

		[[nodiscard]] bool IsMasked() const { return Mode == ImportMaterial::AlphaMode::Mask; }

		// Picks Pipeline again after Mode, DoubleSided or Template changed
		void SelectPipeline();

		// Rewrites this material's table row from its current factors and textures
		void WriteRow() const;
		void Release(GfxDevice& device);
	};

	template<>
	struct AssetTraits<Material>
	{
		static constexpr std::string_view Name = "Material";

		using Source = MaterialSource;

		static Result<std::unique_ptr<Source>> Load(const std::string& vfsPath);
		static Material Upload(GfxDevice& device, const Source& source);
		static void Destroy(GfxDevice& device, Material& resource);
	};

}
