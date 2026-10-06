#include "Koi/Engine/Assets/MaterialFile.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Logging/Logger.h"
#include <cstring>
#include <tuple>

namespace koi
{
	namespace
	{
		struct StringTable
		{
			std::string Bytes;

			std::pair<u32, u32> Add(std::string_view text)
			{
				const u32 offset = static_cast<u32>(Bytes.size());
				Bytes += text;
				return { offset, static_cast<u32>(text.size()) };
			}
		};

		std::optional<std::string_view> Slice(std::string_view strings, u32 offset, u32 length)
		{
			if (static_cast<u64>(offset) + length > strings.size()) return std::nullopt;
			return strings.substr(offset, length);
		}

		// Standard template constants: name, type, and the field they live in
		struct ConstantField
		{
			std::string_view  Name;
			MaterialValueType Type;
			size_t            Offset;  // into ImportMaterial::Constants
		};

		constexpr ConstantField kConstants[] =
		{
			{ "base_color",         MaterialValueType::Float4, offsetof(ImportMaterial::Constants, BaseColorFactor)   },
			{ "emissive",           MaterialValueType::Float3, offsetof(ImportMaterial::Constants, EmissiveFactor)    },
			{ "metallic",           MaterialValueType::Float,  offsetof(ImportMaterial::Constants, MetallicFactor)    },
			{ "roughness",          MaterialValueType::Float,  offsetof(ImportMaterial::Constants, RoughnessFactor)   },
			{ "normal_scale",       MaterialValueType::Float,  offsetof(ImportMaterial::Constants, NormalScale)       },
			{ "occlusion_strength", MaterialValueType::Float,  offsetof(ImportMaterial::Constants, OcclusionStrength) },
			{ "alpha_cutoff",       MaterialValueType::Float,  offsetof(ImportMaterial::Constants, AlphaCutoff)       },
		};

		constexpr u32 ComponentCount(MaterialValueType type)
		{
			switch (type)
			{
				case MaterialValueType::Float2: return 2;
				case MaterialValueType::Float3: return 3;
				case MaterialValueType::Float4: return 4;
				case MaterialValueType::Texture: return 0;
				default:                        return 1;
			}
		}
	}

	std::optional<MaterialFileContents> MaterialFileContents::Read(const AssetFile& file, std::string_view debugName)
	{
		ZoneScoped;

		if (file.Type() != AssetFileType::Material)
		{
			KOI_LOG_ERROR("{} isn't a material, it holds a {}", debugName, AssetFile::ToString(file.Type()));
			return std::nullopt;
		}

		const MaterialFileHeader* header = file.ChunkAs<MaterialFileHeader>(MaterialChunk::Header);
		const std::span<const MaterialFileParam> params = file.ChunkArray<MaterialFileParam>(MaterialChunk::Params);
		const std::span<const byte> stringBytes = file.ChunkBytes(MaterialChunk::Strings);
		const std::string_view strings(reinterpret_cast<const char*>(stringBytes.data()), stringBytes.size());

		if (!header || params.size() != header->ParamCount)
		{
			KOI_LOG_ERROR("{} is missing its material header or some of its parameters. Reimport it", debugName);
			return std::nullopt;
		}

		const std::optional<std::string_view> name    = Slice(strings, header->NameOffset, header->NameLength);
		const std::optional<std::string_view> templ   = Slice(strings, header->TemplateOffset, header->TemplateLength);
		if (!name || !templ)
		{
			KOI_LOG_ERROR("{} has names pointing outside its string table. Reimport it", debugName);
			return std::nullopt;
		}

		MaterialFileContents contents
		{
			.Name         = std::string(*name),
			.Template     = std::string(*templ),
			.AlphaMode    = header->AlphaMode,
			.DoubleSided  = header->DoubleSided != 0,
			.ShadingModel = header->ShadingModel,
		};

		contents.Params.reserve(params.size());
		for (const MaterialFileParam& param : params)
		{
			const std::optional<std::string_view> paramName = Slice(strings, param.NameOffset, param.NameLength);
			if (!paramName) continue;

			MaterialFileContents::Param& out = contents.Params.emplace_back();
			out.Name    = std::string(*paramName);
			out.Type    = param.Type;
			out.Texture = UUID(param.Texture);
			std::memcpy(out.Value.data(), param.Value, sizeof(param.Value));
		}

		return contents;
	}

	void MaterialFileContents::Write(AssetFileWriter& writer) const
	{
		StringTable strings;
		MaterialFileHeader header;
		std::tie(header.NameOffset, header.NameLength)         = strings.Add(Name);
		std::tie(header.TemplateOffset, header.TemplateLength) = strings.Add(Template);
		header.AlphaMode    = AlphaMode;
		header.DoubleSided  = DoubleSided ? 1 : 0;
		header.ShadingModel = ShadingModel;
		header.ParamCount   = static_cast<u32>(Params.size());

		std::vector<MaterialFileParam> params;
		params.reserve(Params.size());
		for (const MaterialFileContents::Param& param : Params)
		{
			MaterialFileParam& out = params.emplace_back();
			std::tie(out.NameOffset, out.NameLength) = strings.Add(param.Name);
			out.Type = param.Type;
			std::memcpy(out.Value, param.Value.data(), sizeof(out.Value));
			const auto id = param.Texture.as_bytes();
			std::memcpy(out.Texture.data(), id.data(), out.Texture.size());
		}

		writer.AddChunkValue(MaterialChunk::Header, 1, header);
		writer.AddChunkArray<MaterialFileParam>(MaterialChunk::Params, 1, params);
		writer.AddChunk(MaterialChunk::Strings, 1, { reinterpret_cast<const byte*>(strings.Bytes.data()), strings.Bytes.size() });
	}

	MaterialFileContents MaterialFileContents::FromSource(const MaterialSource& source, const TextureIdFor& idFor)
	{
		MaterialFileContents contents
		{
			.Name        = source.Name,
			.Template    = source.Template,
			.AlphaMode   = static_cast<u8>(source.Mode),
			.DoubleSided = source.DoubleSided,
		};

		for (const ConstantField& field : kConstants)
		{
			MaterialFileContents::Param& param = contents.Params.emplace_back();
			param.Name = std::string(field.Name);
			param.Type = field.Type;
			std::memcpy(param.Value.data(), reinterpret_cast<const byte*>(&source.Constants) + field.Offset, ComponentCount(field.Type) * sizeof(f32));
		}

		for (size_t i = 0; i < kMaterialTextureCount; ++i)
		{
			if (!source.Textures[i].IsValid()) continue;

			MaterialFileContents::Param& param = contents.Params.emplace_back();
			param.Name    = std::string(Material::SlotName(static_cast<MaterialTextureSlot>(i)));
			param.Type    = MaterialValueType::Texture;
			param.Texture = idFor(source.Textures[i]);
		}

		return contents;
	}

	MaterialSource MaterialFileContents::ToSource(const TextureKeyFor& keyFor) const
	{
		MaterialSource source;
		source.Name        = Name;
		source.Template    = Template;
		source.Mode        = static_cast<ImportMaterial::AlphaMode>(AlphaMode);
		source.DoubleSided = DoubleSided;

		for (const MaterialFileContents::Param& param : Params)
		{
			if (param.Type == MaterialValueType::Texture)
			{
				for (size_t i = 0; i < kMaterialTextureCount; ++i)
				{
					if (param.Name == Material::SlotName(static_cast<MaterialTextureSlot>(i)))
						source.Textures[i] = keyFor(param.Texture);
				}
				continue;
			}

			// Unknown names are kept quiet: a newer template's parameters just don't apply here yet
			for (const ConstantField& field : kConstants)
			{
				if (field.Name != param.Name || field.Type != param.Type) continue;
				std::memcpy(reinterpret_cast<byte*>(&source.Constants) + field.Offset, param.Value.data(), ComponentCount(field.Type) * sizeof(f32));
			}
		}

		return source;
	}
}
