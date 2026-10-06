#pragma once
#include "Koi/Editor/Assets/ImportProgress.h"
#include "Koi/Engine/Assets/AssetFile.h"
#include "Koi/Engine/Graphics/Format.h"

namespace koi
{
	struct ImageAsset;
}

namespace koi::editor
{
	// What a texture holds, which picks its format and colour space
	enum class TextureKind : u8
	{
		Color = 0,  // albedo, emissive: sRGB, BC7
		Linear,     // packed data like metallic/roughness/AO: BC7
		NormalMap,  // BC5, two channels, z rebuilt in the shader
		Grayscale,  // one channel: BC4
		HDR,        // .hdr environment maps and friends: BC6H
		COUNT
	};

	[[nodiscard]] std::string_view ToString(TextureKind kind);

	// Stored verbatim in the .ktex, so a reimport makes the same choices
	struct TextureImportSettings
	{
		TextureKind Kind         = TextureKind::Color;
		u8          GenerateMips = 1;
		u8          Compress     = 1;
		u8          Reserved0    = 0;  // was BestQuality: CPU BC7 at anything above quick mode took about a minute
		u32         Reserved[3]  = {};
	};
	static_assert(sizeof(TextureImportSettings) == 16 && std::is_trivially_copyable_v<TextureImportSettings>);

	inline constexpr u32 kTextureImporterVersion = 1;

	[[nodiscard]] bool IsTextureSource(const fs::path& path);

	// Judged from the file name alone, so a big batch can be listed without decoding anything
	[[nodiscard]] TextureImportSettings GuessTextureSettings(const fs::path& source);

	[[nodiscard]] Format OutputFormatFor(const TextureImportSettings& settings);
	[[nodiscard]] std::string_view DescribeFormat(Format format);  // "BC7 sRGB", for the UI

	struct TextureImportRequest
	{
		fs::path              Source;
		std::string           SourceRef;  // what the .ktex remembers as its source
		fs::path              Output;
		UUID                  Id;
		TextureImportSettings Settings;
		const ImageAsset*     Decoded = nullptr;  // RGBA8 pixels already in memory (glTF-embedded), used instead of Source
	};

	// Runs on an import worker thread. Fills outError on failure
	[[nodiscard]] bool ImportTexture(const TextureImportRequest& request, ImportProgress& progress, std::string& outError);
}
