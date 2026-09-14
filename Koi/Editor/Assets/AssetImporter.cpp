#include "Koi/Editor/Assets/AssetImporter.h"
#include "Koi/Editor/Assets/AssetDatabase.h"
#include "Koi/Editor/Assets/MaterialFile.h"
#include "Koi/Engine/Graphics/MeshLoader.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Memory/MemoryTracker.h"
#include <toml++/toml.hpp>
#include <fstream>
#include <unordered_set>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace koi::editor
{
	namespace
	{
		constexpr std::string_view kProjectScheme = "project://";

		// A material's name reaches the filesystem, so anything that cannot appear in a path goes.
		// glTF does not require material names, and Sponza gives none at all, so the index is
		// what keeps them apart rather than a run of suffixes
		static std::string SanitizedName(std::string_view name, size_t index)
		{
			constexpr size_t kMaxName = 48;

			std::string out;
			out.reserve(std::min(name.size(), kMaxName));

			for (const char c : name)
			{
				if (out.size() == kMaxName) break;
				out.push_back(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' ? c : '_');
			}

			return out.empty() ? "Material_" + std::to_string(index) : out;
		}

		static bool WriteToml(const fs::path& file, const toml::table& table)
		{
			std::ofstream out(file, std::ios::trunc);
			if (!out)
			{
				KOI_LOG_ERROR("Import: cannot write {}", file.string());
				return false;
			}

			out << table << '\n';
			return true;
		}

		// Embedded images have no file of their own
		static std::string ExtractImage(const ImageAsset& image, size_t index, const fs::path& meshDir, const std::string& stem, const std::string& virtualDir)
		{
			if (image.Pixels.empty()) return image.Path;

			const std::string name = stem + "_" + std::to_string(index) + ".png";
			const fs::path    file = meshDir / name;

			const int stride = image.Width * 4;
			if (!stbi_write_png(file.string().c_str(), image.Width, image.Height, 4, image.Pixels.data(), stride))
			{
				KOI_LOG_ERROR("Import: failed to write {}", file.string());
				return {};
			}

			KOI_LOG_INFO("Import: extracted {}", name);
			return virtualDir + "/" + name;
		}

		// The uuid the database gave a path, or the path itself when it tracks no such asset
		static std::string KeyTextFor(const AssetDatabase& database, const std::string& virtualPath)
		{
			if (virtualPath.empty()) return {};

			const AssetEntry* entry = database.Find(virtualPath);
			return entry && !entry->Id.is_nil() ? ToString(entry->Id) : virtualPath;
		}

		MaterialSource SourceFor(const ImportMaterial& material, const AssetDatabase& database,
		                         const std::vector<std::string>& imagePaths)
		{
			MaterialSource source
			{
				.Name        = material.Name,
				.Constants   = material.MaterialConstants,
				.Mode        = material.Mode,
				.DoubleSided = material.DoubleSided,
			};

			// glTF texture members are indices into the model's image list, not descriptors
			const std::array<DescriptorIndex, kMaterialTextureCount> indices =
			{
				material.BaseColorTextureIdx,
				material.MetallicRoughnessTextureIdx,
				material.NormalTextureIdx,
				material.OcclusionTextureIdx,
				material.EmissiveTextureIdx,
			};

			for (size_t i = 0; i < kMaterialTextureCount; ++i)
			{
				if (indices[i] >= imagePaths.size()) continue;

				const std::string key = KeyTextFor(database, imagePaths[indices[i]]);
				if (!key.empty()) source.Textures[i] = AssetKey::Parse(key);
			}

			return source;
		}
	}

	bool ImportMesh(AssetDatabase& database, const UUID& meshId)
	{
		KOI_MEM_TAG_SCOPE(MemTag::Editor);

		const AssetEntry* mesh = database.Find(meshId);
		if (!mesh || mesh->Kind != AssetKind::Mesh)
		{
			KOI_LOG_ERROR("Import: {} is not a mesh asset", ToString(meshId));
			return false;
		}

		const std::unique_ptr<ModelAsset> model = MeshLoader::Load(mesh->DiskPath);
		if (!model)
		{
			KOI_LOG_ERROR("Import: {} did not load", mesh->VirtualPath);
			return false;
		}

		// Everything the entry carries is copied out here: each Refresh below rebuilds the
		// database's entry list, which leaves any AssetEntry pointer dangling
		const fs::path    meshDir     = mesh->DiskPath.parent_path();
		const std::string stem        = mesh->DiskPath.stem().string();
		const std::string virtualDir  = mesh->Folder;
		const std::string meshPath    = mesh->VirtualPath;
		const std::string meshName    = mesh->Name;
		mesh = nullptr;

		std::vector<std::string> imagePaths;
		imagePaths.reserve(model->Images.size());
		for (size_t i = 0; i < model->Images.size(); ++i)
		{
			imagePaths.push_back(ExtractImage(model->Images[i], i, meshDir, stem, virtualDir));
		}

		// Extracted textures need uuids before a material can reference them by one
		database.Refresh();

		std::vector<std::string> materialPaths;
		materialPaths.reserve(model->Materials.size());

		std::unordered_set<std::string> used;
		for (size_t i = 0; i < model->Materials.size(); ++i)
		{
			const ImportMaterial& material = model->Materials[i];

			// Two materials may still share a name; the index breaks the tie without growing
			std::string name = stem + "." + SanitizedName(material.Name, i);
			if (!used.insert(name).second)
			{
				name += "_" + std::to_string(i);
				used.insert(name);
			}

			const std::string file = name + std::string(internal::kMaterialExt[0]);
			if (!WriteMaterial(meshDir / file, SourceFor(material, database, imagePaths))) return false;

			materialPaths.push_back(virtualDir + "/" + file);
		}

		// And the materials need theirs before the sidecar can bind them to slots
		database.Refresh();

		toml::array slots;
		for (size_t i = 0; i < materialPaths.size(); ++i)
		{
			const std::string key = KeyTextFor(database, materialPaths[i]);
			if (key.empty())
				continue;

			toml::table slot;
			slot.insert("index",    static_cast<i64>(i));
			slot.insert("material", key);
			slots.push_back(std::move(slot));
		}

		toml::table sidecar;
		sidecar.insert("source", meshPath);
		sidecar.insert("slot",   std::move(slots));

		if (!WriteToml(meshDir / (stem + std::string(kMeshSidecarExt)), sidecar)) return false;

		database.Refresh();

		KOI_LOG_INFO("Import: {} produced {} material(s) and {} texture(s)",
		             meshName, materialPaths.size(), model->Images.size());
		return true;
	}
}
