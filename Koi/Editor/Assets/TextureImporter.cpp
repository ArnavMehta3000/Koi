#include "Koi/Editor/Assets/TextureImporter.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Logging/Logger.h"
#include <d3d11.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <DirectXTex.h>
#include <objbase.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>

namespace koi::editor
{
	namespace
	{
		using namespace DirectX;
		using namespace std::string_view_literals;

		constexpr std::array kSourceExtensions = { ".png"sv, ".jpg"sv, ".jpeg"sv, ".tga"sv, ".bmp"sv, ".tif"sv, ".tiff"sv, ".gif"sv, ".dds"sv, ".hdr"sv };

		std::string Lower(std::string text)
		{
			std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return text;
		}

		u64 HashBytes(std::span<const byte> bytes)
		{
			u64 hash = 14695981039346656037ull;  // FNV-1a
			for (const byte b : bytes)
			{
				hash ^= b;
				hash *= 1099511628211ull;
			}
			return hash;
		}

		// DirectXTex makes one WIC factory per process, living in COM's multithreaded apartment. A thread that
		// left the apartment again could take it down with the factory still cached, so join once and stay
		void JoinComApartment()
		{
			thread_local const HRESULT joined = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
			(void)joined;
		}

		HRESULT Decode(const fs::path& source, std::span<const byte> bytes, const TextureImportSettings& settings, ScratchImage& out)
		{
			const std::string ext = Lower(source.extension().string());
			ZoneScoped;
			if (ext == ".dds") return LoadFromDDSMemory(bytes.data(), bytes.size(), DDS_FLAGS_NONE, nullptr, out);
			if (ext == ".hdr") return LoadFromHDRMemory(bytes.data(), bytes.size(), nullptr, out);
			if (ext == ".tga") return LoadFromTGAMemory(bytes.data(), bytes.size(), TGA_FLAGS_NONE, nullptr, out);

			// Data textures are often tagged sRGB by whatever saved them, which they aren't
			const WIC_FLAGS flags = settings.Kind == TextureKind::Color ? WIC_FLAGS_NONE : WIC_FLAGS_IGNORE_SRGB;
			return LoadFromWICMemory(bytes.data(), bytes.size(), flags, nullptr, out);
		}

		// The format filtering and mip generation happen in, before any compression
		DXGI_FORMAT WorkingFormatFor(TextureKind kind)
		{
			switch (kind)
			{
				case TextureKind::Color:     return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
				case TextureKind::Grayscale: return DXGI_FORMAT_R8_UNORM;
				case TextureKind::HDR:       return DXGI_FORMAT_R32G32B32A32_FLOAT;
				default:                     return DXGI_FORMAT_R8G8B8A8_UNORM;
			}
		}

		// Reinterprets 8-bit RGBA as sRGB or linear without touching the values, then converts to the working format
		HRESULT ToWorkingFormat(ScratchImage& image, TextureKind kind)
		{
			const DXGI_FORMAT target = WorkingFormatFor(kind);
			if (image.GetMetadata().format == target) return S_OK;

			if (kind != TextureKind::HDR)
			{
				const DXGI_FORMAT current = image.GetMetadata().format;
				if (current != DXGI_FORMAT_R8G8B8A8_UNORM && current != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
				{
					ScratchImage rgba;
					const HRESULT hr = Convert(
						image.GetImages(),
						image.GetImageCount(),
						image.GetMetadata(),
						IsSRGB(current) ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM,
						TEX_FILTER_DEFAULT,
						TEX_THRESHOLD_DEFAULT,
						rgba);

					if (FAILED(hr)) return hr;
					image = std::move(rgba);
				}

				// A PNG without an sRGB tag still holds sRGB colour, so this is a relabel, not a conversion
				image.OverrideFormat(kind == TextureKind::Color ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM);
				if (image.GetMetadata().format == target) return S_OK;
			}

			ScratchImage converted;
			const HRESULT hr = Convert(image.GetImages(), image.GetImageCount(), image.GetMetadata(), target, TEX_FILTER_DEFAULT, TEX_THRESHOLD_DEFAULT, converted);
			if (SUCCEEDED(hr)) image = std::move(converted);
			return hr;
		}

		std::string HResultText(HRESULT hr)
		{
			char buffer[16];
			std::snprintf(buffer, sizeof(buffer), "0x%08X", static_cast<unsigned>(hr));
			return buffer;
		}

		// DirectXTex only compresses BC6H/BC7 on the GPU through D3D11, so the importer keeps its own device
		struct GpuCompressor
		{
			Microsoft::WRL::ComPtr<ID3D11Device> Device;
			std::mutex                           Mutex;
		};

		GpuCompressor* GetGpuCompressor()
		{
			static GpuCompressor* compressor = []() -> GpuCompressor*
			{
				Microsoft::WRL::ComPtr<IDXGIFactory6> factory;
				if (FAILED(::CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return nullptr;

				constexpr UINT kWarpVendorId = 0x1414;
				constexpr D3D_FEATURE_LEVEL kLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
				Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
				for (UINT i = 0; factory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND; ++i)
				{
					DXGI_ADAPTER_DESC1 desc{};
					adapter->GetDesc1(&desc);
					if (desc.VendorId == kWarpVendorId || (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) continue;

					Microsoft::WRL::ComPtr<ID3D11Device> device;
					if (FAILED(::D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, kLevels, static_cast<UINT>(std::size(kLevels)),
						D3D11_SDK_VERSION, &device, nullptr, nullptr)))
						continue;

					char name[128];
					std::snprintf(name, sizeof(name), "%ls", desc.Description);
					KOI_LOG_INFO("Texture importer: BC6H/BC7 compress on the GPU ({})", name);
					return new GpuCompressor{ .Device = std::move(device) };
				}

				KOI_LOG_WARN("Texture importer: no D3D11 GPU found, BC6H/BC7 compress on the CPU");
				return nullptr;
			}();
			return compressor;
		}

		HRESULT CompressMip(const Image& src, DXGI_FORMAT format, ScratchImage& out)
		{
			const bool gpuFormat = format == DXGI_FORMAT_BC7_UNORM
				|| format == DXGI_FORMAT_BC7_UNORM_SRGB
				|| format == DXGI_FORMAT_BC6H_UF16
				|| format == DXGI_FORMAT_BC6H_SF16;

			if (GpuCompressor* gpu = gpuFormat ? GetGpuCompressor() : nullptr)
			{
				std::scoped_lock lock(gpu->Mutex);
				const HRESULT hr = Compress(gpu->Device.Get(), src, format, TEX_COMPRESS_DEFAULT, 1.0f, out);
				if (SUCCEEDED(hr)) return hr;
				KOI_LOG_WARN("Texture importer: GPU compression failed ({}), retrying on the CPU", HResultText(hr));
			}

			// BC7 stays in quick mode on the CPU
			return Compress(src, format, TEX_COMPRESS_PARALLEL | TEX_COMPRESS_BC7_QUICK, TEX_THRESHOLD_DEFAULT, out);
		}
	}

	std::string_view ToString(TextureKind kind)
	{
		switch (kind)
		{
			case TextureKind::Color:     return "Color";
			case TextureKind::Linear:    return "Linear";
			case TextureKind::NormalMap: return "Normal map";
			case TextureKind::Grayscale: return "Grayscale";
			case TextureKind::HDR:       return "HDR";
			default:                     return "Unknown";
		}
	}

	bool IsTextureSource(const fs::path& path)
	{
		return std::ranges::contains(kSourceExtensions, Lower(path.extension().string()));
	}

	TextureImportSettings GuessTextureSettings(const fs::path& source)
	{
		const std::string stem = Lower(source.stem().string());
		const std::string ext  = Lower(source.extension().string());
		const auto has = [&](std::initializer_list<std::string_view> words)
		{
			return std::ranges::any_of(words, [&](std::string_view w) { return stem.contains(w); });
		};

		TextureImportSettings settings;
		if (ext == ".hdr")
			settings.Kind = TextureKind::HDR;
		else if (has({ "normal", "_nrm", "_nor" }) || stem.ends_with("_n"))
			settings.Kind = TextureKind::NormalMap;
		else if (has({ "rough", "metal", "_orm", "_arm", "_ao", "occlusion", "mask", "spec", "height", "disp" }))
			settings.Kind = TextureKind::Linear;

		return settings;
	}

	Format OutputFormatFor(const TextureImportSettings& settings)
	{
		const bool bc = settings.Compress != 0;
		switch (settings.Kind)
		{
			case TextureKind::Color:     return bc ? Format::BC7_UNORM_SRGB : Format::R8G8B8A8_UNORM_SRGB;
			case TextureKind::Linear:    return bc ? Format::BC7_UNORM      : Format::R8G8B8A8_UNORM;
			case TextureKind::NormalMap: return bc ? Format::BC5_UNORM      : Format::R8G8B8A8_UNORM;
			case TextureKind::Grayscale: return bc ? Format::BC4_UNORM      : Format::R8_UNORM;
			case TextureKind::HDR:       return bc ? Format::BC6H_UF16      : Format::R16G16B16A16_FLOAT;
			default:                     return Format::UNKNOWN;
		}
	}

	std::string_view DescribeFormat(Format format)
	{
		switch (format)
		{
			case Format::BC7_UNORM_SRGB:      return "BC7 sRGB";
			case Format::BC7_UNORM:           return "BC7";
			case Format::BC5_UNORM:           return "BC5";
			case Format::BC4_UNORM:           return "BC4";
			case Format::BC6H_UF16:           return "BC6H";
			case Format::R8G8B8A8_UNORM_SRGB: return "RGBA8 sRGB";
			case Format::R8G8B8A8_UNORM:      return "RGBA8";
			case Format::R8_UNORM:            return "R8";
			case Format::R16G16B16A16_FLOAT:  return "RGBA16F";
			default:                          return "?";
		}
	}

	bool ImportTexture(const TextureImportRequest& request, ImportProgress& progress, std::string& outError)
	{
		ZoneScoped;
		JoinComApartment();

		const auto fail = [&](std::string message) { outError = std::move(message); return false; };
		const auto cancelled = [&] { return progress.IsCancelled(); };
		const TextureImportSettings& settings = request.Settings;

		progress.BeginStage("read");
		progress.Report("Reading", 0.02f);
		std::vector<byte> bytes;
		if (!request.Decoded)
		{
			ZoneScopedN("Read source");
			std::ifstream file(request.Source, std::ios::binary | std::ios::ate);
			if (!file) return fail("couldn't open the source file");
			bytes.resize(static_cast<size_t>(file.tellg()));
			file.seekg(0);
			if (!file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
				return fail("couldn't read the source file");
		}

		progress.BeginStage("decode");
		progress.Report("Decoding", 0.05f);
		ScratchImage image;
		if (const ImageAsset* decoded = request.Decoded)
		{
			if (decoded->Width <= 0 || decoded->Height <= 0 || decoded->Pixels.size() < static_cast<size_t>(decoded->Width) * decoded->Height * 4)
				return fail("the embedded image has no usable pixels");

			if (const HRESULT hr = image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, decoded->Width, decoded->Height, 1, 1); FAILED(hr))
				return fail("couldn't allocate the image (" + HResultText(hr) + ")");

			const Image& dst = *image.GetImage(0, 0, 0);
			const size_t rowBytes = static_cast<size_t>(decoded->Width) * 4;
			for (i32 row = 0; row < decoded->Height; ++row)
			{
				std::memcpy(dst.pixels + row * dst.rowPitch, decoded->Pixels.data() + row * rowBytes, rowBytes);
			}

			bytes = decoded->Pixels;  // what the source hash covers
		}
		else if (const HRESULT hr = Decode(request.Source, bytes, settings, image); FAILED(hr))
		{
			return fail("couldn't decode it (" + HResultText(hr) + "). Is it really an image?");
		}

		const TexMetadata& source = image.GetMetadata();
		if (source.arraySize > 1 || source.depth > 1 || source.IsCubemap())
			return fail("arrays, cubemaps and volume textures aren't supported yet, only plain 2D images");

		if (source.mipLevels > 1 || IsCompressed(source.format))
		{
			// Start over from the top mip, uncompressed: mips get rebuilt and compression redone anyway
			ScratchImage top;
			HRESULT hr = IsCompressed(source.format)
				? Decompress(*image.GetImage(0, 0, 0), DXGI_FORMAT_UNKNOWN, top)
				: top.InitializeFromImage(*image.GetImage(0, 0, 0));
			if (FAILED(hr)) return fail("couldn't unpack the existing DDS data (" + HResultText(hr) + ")");
			image = std::move(top);
		}

		if (cancelled()) return fail("cancelled");

		if (const HRESULT hr = ToWorkingFormat(image, settings.Kind); FAILED(hr))
			return fail("couldn't convert it to a format the importer works in (" + HResultText(hr) + ")");

		const bool hasAlpha = (settings.Kind == TextureKind::Color || settings.Kind == TextureKind::Linear) && !image.IsAlphaAllOpaque();

		const TexMetadata& meta = image.GetMetadata();
		if (settings.GenerateMips && (meta.width > 1 || meta.height > 1))
		{
			ZoneScopedN("Generate mips");
			progress.BeginStage("mips");
			progress.Report("Generating mips", 0.15f);
			ScratchImage chain;
			if (const HRESULT hr = GenerateMipMaps(image.GetImages(), image.GetImageCount(), meta, TEX_FILTER_DEFAULT, 0, chain); FAILED(hr))
				return fail("mip generation failed (" + HResultText(hr) + ")");
			image = std::move(chain);
		}

		if (cancelled()) return fail("cancelled");

		const Format outFormat    = OutputFormatFor(settings);
		const DXGI_FORMAT outDxgi = static_cast<DXGI_FORMAT>(outFormat);
		const TexMetadata& final  = image.GetMetadata();
		const u32 width           = static_cast<u32>(final.width);
		const u32 height          = static_cast<u32>(final.height);
		const u32 mips            = static_cast<u32>(final.mipLevels);

		std::vector<TextureSubresourceLayout> layout;
		u64 totalBytes = 0;
		if (!ComputeUploadLayout(outFormat, width, height, mips, 1, layout, totalBytes))
			return fail("the importer doesn't know how to lay out " + std::string(DescribeFormat(outFormat)));

		std::vector<byte> data(totalBytes, 0);
		const auto copyMip = [&](u32 mip, const Image& src)
		{
			const TextureSubresourceLayout& dst = layout[mip];
			const size_t rowBytes = std::min<size_t>(dst.RowBytes, src.rowPitch);
			for (u32 row = 0; row < dst.NumRows; ++row)
				std::memcpy(data.data() + dst.Offset + static_cast<u64>(row) * dst.RowPitch, src.pixels + row * src.rowPitch, rowBytes);
		};

		if (IsCompressed(outDxgi))
		{
			// One mip at a time, so the progress bar moves. Pixel counts weight it: mip 0 is three quarters of the work
			progress.BeginStage("compress");
			const f64 totalPixels = static_cast<f64>(width) * height * 4.0 / 3.0;
			f64 donePixels = 0.0;

			for (u32 mip = 0; mip < mips; ++mip)
			{
				if (cancelled()) return fail("cancelled");

				ZoneScopedN("Compress mip");
				const Image& src = *image.GetImage(mip, 0, 0);
				progress.Report("Compressing to " + std::string(DescribeFormat(outFormat)) + " (mip " + std::to_string(mip + 1) + "/" + std::to_string(mips) + ")",
					0.2f + 0.75f * static_cast<f32>(donePixels / totalPixels));

				ScratchImage compressed;
				if (const HRESULT hr = CompressMip(src, outDxgi, compressed); FAILED(hr))
					return fail("compression failed on mip " + std::to_string(mip) + " (" + HResultText(hr) + ")");

				copyMip(mip, *compressed.GetImage(0, 0, 0));
				donePixels += static_cast<f64>(src.width) * src.height;
			}
		}
		else
		{
			progress.BeginStage("convert");
			progress.Report("Converting", 0.5f);
			if (final.format != outDxgi)
			{
				ScratchImage converted;
				if (const HRESULT hr = Convert(image.GetImages(), image.GetImageCount(), final, outDxgi, TEX_FILTER_DEFAULT, TEX_THRESHOLD_DEFAULT, converted); FAILED(hr))
					return fail("conversion to " + std::string(DescribeFormat(outFormat)) + " failed (" + HResultText(hr) + ")");
				image = std::move(converted);
			}

			for (u32 mip = 0; mip < mips; ++mip)
				copyMip(mip, *image.GetImage(mip, 0, 0));
		}

		if (cancelled()) return fail("cancelled");
		progress.BeginStage("write");
		progress.Report("Writing", 0.97f);
		ZoneNamedN(writeZone, "Write .ktex", true);

		u32 flags = 0;
		if (settings.Kind == TextureKind::Color)     flags |= static_cast<u32>(TextureFileFlags::SRGB);
		if (settings.Kind == TextureKind::NormalMap) flags |= static_cast<u32>(TextureFileFlags::NormalMap);
		if (hasAlpha)                                flags |= static_cast<u32>(TextureFileFlags::HasAlpha);

		const TextureHeader header{ .Format = outFormat, .Width = width, .Height = height, .MipLevels = mips, .ArraySize = 1, .Flags = flags };

		AssetFileWriter writer(AssetFileType::Texture, request.Id);
		writer.SetSourceHash(HashBytes(bytes));
		writer.SetImporterVersion(kTextureImporterVersion);
		writer.AddChunkValue(TextureChunk::Header, 1, header);
		writer.AddChunkArray<TextureSubresourceLayout>(TextureChunk::Subresources, 1, layout);
		writer.AddChunk(TextureChunk::Data, 1, data);
		writer.AddChunkValue(TextureChunk::ImportSettings, 1, settings, ChunkFlags::EditorOnly);
		writer.AddChunk(TextureChunk::SourcePath, 1, { reinterpret_cast<const byte*>(request.SourceRef.data()), request.SourceRef.size() }, ChunkFlags::EditorOnly);

		if (!writer.Save(request.Output))
			return fail("couldn't write " + request.Output.string());

		progress.EndStage();
		progress.Report("Done", 1.0f);
		return true;
	}
}
