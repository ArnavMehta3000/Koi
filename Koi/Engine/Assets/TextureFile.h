#pragma once
#include "Koi/Engine/Assets/AssetFile.h"
#include "Koi/Engine/Graphics/Types.h"

namespace koi
{
	namespace TextureChunk
	{
		inline constexpr u32 Header         = utils::FourCC("TXHD");  // TextureHeader
		inline constexpr u32 Subresources   = utils::FourCC("TXSR");  // TextureSubresourceLayout[], mip-major per array slice
		inline constexpr u32 Data           = utils::FourCC("TXDT");  // every subresource, already in upload layout
		inline constexpr u32 ImportSettings = utils::FourCC("IMPS");  // editor only
		inline constexpr u32 SourcePath     = utils::FourCC("SRCP");  // editor only, UTF-8
	}

	enum class TextureFileFlags : u32
	{
		None      = 0,
		SRGB      = 1 << 0,  // colour data, the format is an _SRGB one
		NormalMap = 1 << 1,
		HasAlpha  = 1 << 2,
	};

	struct TextureHeader
	{
		Format Format     = Format::UNKNOWN;
		u32    Width      = 0;
		u32    Height     = 0;
		u32    MipLevels  = 1;
		u32    ArraySize  = 1;
		u32    Flags      = 0;  // TextureFileFlags
		u32    Reserved[2] = {};
	};
	static_assert(sizeof(TextureHeader) == 32 && std::is_trivially_copyable_v<TextureHeader>);

	// A .ktex held in memory, with views into its chunks
	struct PreparedTexture
	{
		AssetFile                                 File;
		const TextureHeader*                      Header = nullptr;
		std::span<const TextureSubresourceLayout> Subresources;
		std::span<const byte>                     Data;

		[[nodiscard]] bool Has(TextureFileFlags flag) const { return (Header->Flags & static_cast<u32>(flag)) != 0; }

		// Checks the chunks agree with each other before anything gets near the GPU
		[[nodiscard]] static Result<std::shared_ptr<const PreparedTexture>> Read(AssetFile file);

		// The layout D3D12's GetCopyableFootprints produces: rows padded to 256 bytes, subresources 512-byte aligned.
		// False if the format isn't one Koi knows the block size of
		[[nodiscard]] static bool ComputeUploadLayout(Format format, u32 width, u32 height, u32 mipLevels, u32 arraySize,
			std::vector<TextureSubresourceLayout>& outLayout, u64& outTotalBytes);
	};
}
