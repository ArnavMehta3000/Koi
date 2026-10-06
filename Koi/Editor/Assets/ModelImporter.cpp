#include "Koi/Editor/Assets/ModelImporter.h"
#include "Koi/Editor/Assets/MeshLoader.h"
#include "Koi/Editor/Assets/TextureImporter.h"
#include "Koi/Engine/Assets/MaterialFile.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/IO/VirtualFileSystem.h"
#include "Koi/Engine/Logging/Logger.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <format>
#include <fstream>

namespace koi::editor
{
	namespace
	{
		std::string Lower(std::string text)
		{
			std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return text;
		}

		// Something Windows will accept as a file name
		std::string SafeName(std::string_view name, std::string_view fallback)
		{
			std::string out;
			for (const char c : name)
				out += (static_cast<unsigned char>(c) < 0x20 || std::string_view("<>:\"/\\|?*").contains(c)) ? '_' : c;

			while (!out.empty() && (out.back() == ' ' || out.back() == '.')) out.pop_back();
			return out.empty() ? std::string(fallback) : out;
		}

		// Names that don't collide with anything else this import writes
		struct NameSet
		{
			std::vector<std::string> Taken;

			std::string Claim(const std::string& base)
			{
				std::string name = base;
				for (int n = 2; std::ranges::contains(Taken, Lower(name)); ++n)
					name = base + "_" + std::to_string(n);
				Taken.push_back(Lower(name));
				return name;
			}
		};

		// Reimporting over a file keeps its id, so whatever points at it keeps working
		UUID IdFor(const fs::path& file)
		{
			std::error_code ec;
			const std::optional<AssetFileHeader> existing = fs::exists(file, ec) ? ReadAssetFileHeader(file) : std::nullopt;
			return existing ? UUID(existing->Id) : NewUUID();
		}

		void StoreId(std::array<byte, 16>& out, const UUID& id)
		{
			const auto bytes = id.as_bytes();
			std::memcpy(out.data(), bytes.data(), out.size());
		}

		u64 HashFile(const fs::path& file)
		{
			std::ifstream in(file, std::ios::binary);
			u64 hash = 14695981039346656037ull;  // FNV-1a
			char buffer[64 * 1024];
			while (in.read(buffer, sizeof(buffer)) || in.gcount() > 0)
			{
				for (std::streamsize i = 0; i < in.gcount(); ++i)
				{
					hash ^= static_cast<byte>(buffer[i]);
					hash *= 1099511628211ull;
				}
			}
			return hash;
		}

		TextureKind KindForSlot(MaterialTextureSlot slot)
		{
			switch (slot)
			{
				case MaterialTextureSlot::BaseColor:
				case MaterialTextureSlot::Emissive:  return TextureKind::Color;
				case MaterialTextureSlot::Normal:    return TextureKind::NormalMap;
				default:                             return TextureKind::Linear;  // metallic/roughness, occlusion
			}
		}

		// The image each material slot samples, in MaterialTextureSlot order
		std::array<DescriptorIndex, kMaterialTextureCount> SlotImages(const ImportMaterial& material)
		{
			return { material.BaseColorTextureIdx, material.MetallicRoughnessTextureIdx, material.NormalTextureIdx,
			         material.OcclusionTextureIdx, material.EmissiveTextureIdx };
		}

		struct TextureJob
		{
			u32         Image = 0;
			TextureKind Kind  = TextureKind::Color;
			std::string Name;
			fs::path    File;
			std::string VirtualPath;
			UUID        Id;
			bool        Imported = false;
		};
	}

	bool IsModelSource(const fs::path& path)
	{
		const std::string ext = Lower(path.extension().string());
		return ext == ".gltf" || ext == ".glb";
	}

	bool ImportModel(const ModelImportRequest& request, ImportProgress& progress, std::string& outError)
	{
		ZoneScoped;

		const auto fail = [&](std::string message) { outError = std::move(message); return false; };
		const ModelImportSettings& settings = request.Settings;

		progress.BeginStage("parse");
		progress.Report("Reading the glTF", 0.01f);
		std::unique_ptr<ModelAsset> model;
		{
			ZoneScopedN("Parse glTF");
			model = MeshLoader::Load(request.Source, true);
		}
		if (!model || model->Meshes.empty())
			return fail("fastgltf couldn't read it, or there are no meshes in it (the log has the details)");
		if (progress.IsCancelled()) return fail("cancelled");

		std::error_code ec;
		fs::create_directories(request.Folder, ec);

		// Every image a material samples becomes a .ktex, its kind decided by the slot that first uses it
		std::vector<TextureJob> textures;
		std::vector<i32> jobForImage(model->Images.size(), -1);
		if (settings.ImportMaterials)
		{
			NameSet names;
			for (const ImportMaterial& material : model->Materials)
			{
				const auto images = SlotImages(material);
				for (size_t slot = 0; slot < images.size(); ++slot)
				{
					const DescriptorIndex image = images[slot];
					if (image >= model->Images.size() || jobForImage[image] >= 0) continue;

					TextureJob& job = textures.emplace_back();
					job.Image       = image;
					job.Kind        = KindForSlot(static_cast<MaterialTextureSlot>(slot));
					job.Name        = names.Claim(SafeName(fs::path(model->Images[image].Name).stem().string(), "Texture"));
					job.File        = request.Folder / (job.Name + std::string(ExtensionFor(AssetFileType::Texture)));
					job.VirtualPath = request.VirtualFolder + "/" + job.File.filename().string();
					job.Id          = IdFor(job.File);
					jobForImage[image] = static_cast<i32>(textures.size() - 1);
				}
			}
		}

		// Textures are nearly all of the work, so they get most of the bar
		u32 failedTextures = 0;
		for (size_t i = 0; i < textures.size(); ++i)
		{
			if (progress.IsCancelled()) return fail("cancelled");

			TextureJob& job = textures[i];
			const ImageAsset& image = model->Images[job.Image];
			const f32 span = 0.8f / static_cast<f32>(textures.size());
			progress.SetRange(0.05f + span * i, 0.05f + span * (i + 1), std::format("Texture {}/{} {}: ", i + 1, textures.size(), job.Name));

			TextureImportRequest texture{ .Output = job.File, .Id = job.Id };
			texture.Settings.Kind = job.Kind;
			if (!image.Pixels.empty())
			{
				texture.Decoded   = &image;
				texture.SourceRef = request.SourceRef;  // embedded: only the model can bring it back
			}
			else
			{
				texture.Source    = VirtualFileSystem::Get().TryResolveToDisk(image.Path).value_or(fs::path(image.Path));
				texture.SourceRef = image.Path;

				// Next to a model remembered by virtual path, remember the image the same way rather than by disk path
				const fs::path relative = texture.Source.lexically_relative(request.Source.parent_path());
				const size_t slash = request.SourceRef.find_last_of('/');
				if (request.SourceRef.contains("://") && slash != std::string::npos && !relative.empty() && *relative.begin() != "..")
					texture.SourceRef = request.SourceRef.substr(0, slash + 1) + relative.generic_string();
			}

			std::string error;
			job.Imported = ImportTexture(texture, progress, error);
			if (!job.Imported)
			{
				if (progress.IsCancelled()) return fail("cancelled");
				++failedTextures;
				KOI_LOG_WARN("{}: texture {} didn't import ({}), so its material goes without it", request.Name, job.Name, error);
			}
		}

		progress.SetRange(0.85f, 1.0f);
		progress.BeginStage("materials");
		progress.Report("Writing materials", 0.1f);

		std::vector<UUID> materialIds(model->Materials.size());
		if (settings.ImportMaterials)
		{
			ZoneScopedN("Write materials");
			NameSet names;
			for (size_t i = 0; i < model->Materials.size(); ++i)
			{
				const ImportMaterial& material = model->Materials[i];

				MaterialSource source;
				source.Name        = material.Name.empty() ? std::format("Material_{}", i) : material.Name;
				source.Mode        = material.Mode;
				source.DoubleSided = material.DoubleSided;
				source.Constants   = material.MaterialConstants;

				const auto images = SlotImages(material);
				for (size_t slot = 0; slot < images.size(); ++slot)
				{
					const i32 job = images[slot] < jobForImage.size() ? jobForImage[images[slot]] : -1;
					if (job >= 0 && textures[job].Imported)
						source.Textures[slot] = AssetKey(textures[job].VirtualPath);
				}

				const MaterialFileContents contents = ToMaterialFileContents(source, [&](const AssetKey& key)
				{
					const auto it = std::ranges::find(textures, key.Path(), &TextureJob::VirtualPath);
					return it != textures.end() ? it->Id : UUID();
				});

				const fs::path file = request.Folder / (names.Claim(SafeName(source.Name, "Material")) + std::string(ExtensionFor(AssetFileType::Material)));
				materialIds[i] = IdFor(file);

				AssetFileWriter writer(AssetFileType::Material, materialIds[i]);
				WriteMaterialChunks(writer, contents);
				if (!writer.Save(file)) return fail("couldn't write " + file.filename().string());
			}
		}

		if (progress.IsCancelled()) return fail("cancelled");
		progress.BeginStage("mesh");
		progress.Report("Writing the mesh", 0.5f);
		ZoneNamedN(meshZone, "Write mesh", true);

		// One flat vertex and index buffer, the way the GPU gets it
		const u32 slotCount = static_cast<u32>(model->Materials.size());
		const f32 scale     = settings.Scale > 0.0f ? settings.Scale : 1.0f;

		std::vector<Vertex>      vertices;
		std::vector<u32>         indices;
		std::vector<MeshSubmesh> submeshes;
		Vector3 boundsMin( FLT_MAX), boundsMax(-FLT_MAX);

		for (const Mesh& mesh : model->Meshes)
		{
			const u32 baseVertex = static_cast<u32>(vertices.size());
			const u32 baseIndex  = static_cast<u32>(indices.size());

			for (Vertex vertex : mesh.Vertices)
			{
				vertex.Position *= scale;
				boundsMin = Vector3::Min(boundsMin, vertex.Position);
				boundsMax = Vector3::Max(boundsMax, vertex.Position);
				vertices.push_back(vertex);
			}
			indices.insert(indices.end(), mesh.Indices.begin(), mesh.Indices.end());

			for (const SubMesh& sub : mesh.SubMeshes)
			{
				const bool hasMaterial = sub.MaterialIndex >= 0 && static_cast<u32>(sub.MaterialIndex) < slotCount;
				const Vector3 center  = Vector3(sub.Bounds.Center) * scale;
				const Vector3 extents = Vector3(sub.Bounds.Extents) * scale;
				const Vector3 lo      = center - extents;
				const Vector3 hi      = center + extents;

				MeshSubmesh& out = submeshes.emplace_back();
				out.IndexOffset  = baseIndex + sub.IndexOffset;
				out.IndexCount   = sub.IndexCount;
				out.VertexOffset = static_cast<i32>(baseVertex + sub.VertexOffset);
				out.MaterialSlot = hasMaterial ? static_cast<u32>(sub.MaterialIndex) : slotCount;
				std::memcpy(out.BoundsMin, &lo.x, sizeof(out.BoundsMin));
				std::memcpy(out.BoundsMax, &hi.x, sizeof(out.BoundsMax));
			}
		}

		std::string strings;
		std::vector<MeshMaterialSlot> slots(slotCount);
		for (u32 i = 0; i < slotCount; ++i)
		{
			const std::string& name = model->Materials[i].Name;
			slots[i].NameOffset = static_cast<u32>(strings.size());
			slots[i].NameLength = static_cast<u32>(name.size());
			strings += name;
			StoreId(slots[i].Material, materialIds[i]);
		}

		MeshHeader header;
		header.VertexCount       = static_cast<u32>(vertices.size());
		header.IndexCount        = static_cast<u32>(indices.size());
		header.SubmeshCount      = static_cast<u32>(submeshes.size());
		header.MaterialSlotCount = slotCount;
		std::memcpy(header.BoundsMin, &boundsMin.x, sizeof(header.BoundsMin));
		std::memcpy(header.BoundsMax, &boundsMax.x, sizeof(header.BoundsMax));

		AssetFileWriter writer(AssetFileType::Mesh, request.Id);
		writer.SetSourceHash(HashFile(request.Source));
		writer.SetImporterVersion(kModelImporterVersion);
		writer.AddChunkValue(MeshChunk::Header, 1, header);
		writer.AddChunkArray<Vertex>(MeshChunk::Vertices, 1, vertices);
		writer.AddChunkArray<u32>(MeshChunk::Indices, 1, indices);
		writer.AddChunkArray<MeshSubmesh>(MeshChunk::Submeshes, 1, submeshes);
		writer.AddChunkArray<MeshMaterialSlot>(MeshChunk::MaterialSlots, 1, slots);
		writer.AddChunk(MeshChunk::Strings, 1, { reinterpret_cast<const byte*>(strings.data()), strings.size() });
		writer.AddChunkValue(MeshChunk::ImportSettings, 1, settings, ChunkFlags::EditorOnly);
		writer.AddChunk(MeshChunk::SourcePath, 1, { reinterpret_cast<const byte*>(request.SourceRef.data()), request.SourceRef.size() }, ChunkFlags::EditorOnly);

		const fs::path meshFile = request.Folder / (request.Name + std::string(ExtensionFor(AssetFileType::Mesh)));
		if (!writer.Save(meshFile)) return fail("couldn't write " + meshFile.filename().string());

		progress.EndStage();
		progress.Report("Done", 1.0f);

		if (failedTextures > 0)
			KOI_LOG_WARN("{} imported, but {} of its {} texture(s) didn't. The log above says why", request.Name, failedTextures, textures.size());
		return true;
	}
}
