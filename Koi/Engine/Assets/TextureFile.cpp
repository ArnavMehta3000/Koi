#include "Koi/Engine/Assets/TextureFile.h"
#include "Koi/Engine/Diagnostics/Profiling.h"

namespace koi
{
	namespace
	{
		constexpr u64 kRowPitchAlignment  = 256;  // D3D12_TEXTURE_DATA_PITCH_ALIGNMENT
		constexpr u64 kPlacementAlignment = 512;  // D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT

		constexpr u64 AlignUp(u64 value, u64 alignment)
		{
			return (value + alignment - 1) & ~(alignment - 1);
		}

		struct BlockInfo
		{
			u32  Bytes      = 0;  // per 4x4 block when compressed, per pixel otherwise
			bool Compressed = false;
		};

		BlockInfo GetBlockInfo(Format format)
		{
			switch (static_cast<DXGI_FORMAT>(format))
			{
				case DXGI_FORMAT_BC1_UNORM: case DXGI_FORMAT_BC1_UNORM_SRGB:
				case DXGI_FORMAT_BC4_UNORM: case DXGI_FORMAT_BC4_SNORM:
					return { 8, true };

				case DXGI_FORMAT_BC2_UNORM: case DXGI_FORMAT_BC2_UNORM_SRGB:
				case DXGI_FORMAT_BC3_UNORM: case DXGI_FORMAT_BC3_UNORM_SRGB:
				case DXGI_FORMAT_BC5_UNORM: case DXGI_FORMAT_BC5_SNORM:
				case DXGI_FORMAT_BC6H_UF16: case DXGI_FORMAT_BC6H_SF16:
				case DXGI_FORMAT_BC7_UNORM: case DXGI_FORMAT_BC7_UNORM_SRGB:
					return { 16, true };

				case DXGI_FORMAT_R32G32B32A32_FLOAT:
					return { 16, false };

				case DXGI_FORMAT_R16G16B16A16_FLOAT: case DXGI_FORMAT_R16G16B16A16_UNORM:
					return { 8, false };

				case DXGI_FORMAT_R8G8B8A8_UNORM: case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
				case DXGI_FORMAT_B8G8R8A8_UNORM: case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
				case DXGI_FORMAT_R10G10B10A2_UNORM: case DXGI_FORMAT_R11G11B10_FLOAT:
				case DXGI_FORMAT_R16G16_UNORM: case DXGI_FORMAT_R16G16_FLOAT: case DXGI_FORMAT_R32_FLOAT:
					return { 4, false };

				case DXGI_FORMAT_R8G8_UNORM: case DXGI_FORMAT_R16_UNORM: case DXGI_FORMAT_R16_FLOAT:
					return { 2, false };

				case DXGI_FORMAT_R8_UNORM:
					return { 1, false };

				default:
					return {};
			}
		}
	}

	bool PreparedTexture::ComputeUploadLayout(Format format, u32 width, u32 height, u32 mipLevels, u32 arraySize,
		std::vector<TextureSubresourceLayout>& outLayout, u64& outTotalBytes)
	{
		const BlockInfo block = GetBlockInfo(format);
		if (block.Bytes == 0) return false;

		outLayout.clear();
		outLayout.reserve(static_cast<size_t>(mipLevels) * arraySize);

		u64 cursor = 0;
		for (u32 slice = 0; slice < arraySize; ++slice)
		{
			for (u32 mip = 0; mip < mipLevels; ++mip)
			{
				const u32 w = std::max(width  >> mip, 1u);
				const u32 h = std::max(height >> mip, 1u);

				const u32 columns  = block.Compressed ? std::max((w + 3) / 4, 1u) : w;
				const u32 rows     = block.Compressed ? std::max((h + 3) / 4, 1u) : h;
				const u32 rowBytes = columns * block.Bytes;
				const u32 rowPitch = static_cast<u32>(AlignUp(rowBytes, kRowPitchAlignment));

				const u64 offset = AlignUp(cursor, kPlacementAlignment);
				outLayout.push_back({ .Offset = offset, .RowPitch = rowPitch, .NumRows = rows, .RowBytes = rowBytes });

				// The last row isn't padded out to the pitch, matching GetCopyableFootprints
				cursor = offset + static_cast<u64>(rowPitch) * (rows - 1) + rowBytes;
			}
		}

		outTotalBytes = cursor;
		return true;
	}

	Result<std::shared_ptr<const PreparedTexture>> PreparedTexture::Read(AssetFile file)
	{
		ZoneScoped;

		if (file.Type() != AssetFileType::Texture)
		{
			return Error::Make("it isn't a texture, it holds a {}", AssetFile::ToString(file.Type()));
		}

		auto prepared = std::make_shared<PreparedTexture>(PreparedTexture{ .File = std::move(file) });
		const AssetFile& f = prepared->File;

		prepared->Header       = f.ChunkAs<TextureHeader>(TextureChunk::Header);
		prepared->Subresources = f.ChunkArray<TextureSubresourceLayout>(TextureChunk::Subresources);
		prepared->Data         = f.ChunkBytes(TextureChunk::Data);

		const TextureHeader* header = prepared->Header;
		if (!header || prepared->Subresources.empty() || prepared->Data.empty())
		{
			return Error::Make("it's missing part of its texture data. Reimport it");
		}

		if (header->Width == 0 || header->Height == 0 || header->MipLevels == 0 || header->ArraySize == 0 ||
			prepared->Subresources.size() != static_cast<size_t>(header->MipLevels) * header->ArraySize)
		{
			return Error::Make("it describes a {}x{} texture with {} mip(s) but carries {} subresource(s). Reimport it",
				header->Width, header->Height, header->MipLevels, prepared->Subresources.size());
		}

		for (const TextureSubresourceLayout& sub : prepared->Subresources)
		{
			const u64 end = sub.Offset + static_cast<u64>(sub.RowPitch) * (sub.NumRows - 1) + sub.RowBytes;
			if (sub.NumRows == 0 || sub.RowBytes > sub.RowPitch || end > prepared->Data.size())
			{
				return Error::Make("its mip data doesn't fit inside the file. Reimport it");
			}
		}

		return prepared;
	}
}
