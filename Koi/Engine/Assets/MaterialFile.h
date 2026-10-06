#pragma once
#include "Koi/Engine/Assets/AssetFile.h"
#include "Koi/Engine/Assets/MaterialAsset.h"
#include <functional>

namespace koi
{
	namespace MaterialChunk
	{
		inline constexpr u32 Header  = FourCC("MTHD");  // MaterialFileHeader
		inline constexpr u32 Params  = FourCC("MPRM");  // MaterialFileParam[]
		inline constexpr u32 Strings = FourCC("STRS");  // UTF-8 names, referenced by offset
	}

	// Same order as MaterialParamType, so a template's reflected parameters can map straight onto it later
	enum class MaterialValueType : u8 { Float = 0, Float2, Float3, Float4, Texture };

	struct MaterialFileHeader
	{
		u32 NameOffset     = 0;
		u32 NameLength     = 0;
		u32 TemplateOffset = 0;  // empty picks the renderer's default template
		u32 TemplateLength = 0;
		u8  AlphaMode      = 0;  // ImportMaterial::AlphaMode
		u8  DoubleSided    = 0;
		u8  ShadingModel   = 0;  // 0 = default lit, room for more
		u8  Reserved0      = 0;
		u32 ParamCount     = 0;
		u32 Reserved[2]    = {};
	};
	static_assert(sizeof(MaterialFileHeader) == 32 && std::is_trivially_copyable_v<MaterialFileHeader>);

	struct MaterialFileParam
	{
		u32                  NameOffset = 0;
		u32                  NameLength = 0;
		MaterialValueType    Type       = MaterialValueType::Float;
		u8                   Pad[3]     = {};
		f32                  Value[4]   = {};
		std::array<byte, 16> Texture    = {};  // a .ktex id when Type is Texture
		u32                  Reserved   = 0;
	};
	static_assert(sizeof(MaterialFileParam) == 48 && std::is_trivially_copyable_v<MaterialFileParam>);

	// A .kmat decoded into plain values: template, render state and named parameters
	struct MaterialFileContents
	{
		struct Param
		{
			std::string        Name;
			MaterialValueType  Type = MaterialValueType::Float;
			std::array<f32, 4> Value{};
			UUID               Texture;
		};

		std::string        Name;
		std::string        Template;
		u8                 AlphaMode    = 0;
		bool               DoubleSided  = false;
		u8                 ShadingModel = 0;
		std::vector<Param> Params;
	};

	[[nodiscard]] std::optional<MaterialFileContents> ReadMaterialFile(const AssetFile& file, std::string_view debugName);
	void WriteMaterialChunks(AssetFileWriter& writer, const MaterialFileContents& contents);

	// The bridge to today's fixed-slot MaterialSource, until materials become template-driven
	using TextureIdFor   = std::function<UUID(const AssetKey&)>;
	using TextureKeyFor  = std::function<AssetKey(const UUID&)>;
	[[nodiscard]] MaterialFileContents ToMaterialFileContents(const MaterialSource& source, const TextureIdFor& idFor);
	[[nodiscard]] MaterialSource       ToMaterialSource(const MaterialFileContents& contents, const TextureKeyFor& keyFor);
}
