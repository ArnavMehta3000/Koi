#include "Koi/Engine/Assets/MeshAsset.h"
#include "Koi/Engine/Assets/AssetTypes.h"
#include "Koi/Engine/Graphics/MeshLoader.h"
#include "Koi/Engine/IO/VirtualFileSystem.h"
#include "Koi/Engine/Logging/Logger.h"
#include <toml++/toml.hpp>

namespace koi
{
	namespace
	{
		static void ReadMeshSidecar(const std::string& vfsPath, ModelAsset& model)
		{
			const std::string_view extension = AssetExtensionOf(vfsPath);
			if (extension.empty()) return;

			const std::string sidecarPath = vfsPath.substr(0, vfsPath.size() - extension.size()) + std::string(kMeshSidecarExt);

			const std::optional<fs::path> diskPath = VirtualFileSystem::Get().TryResolveToDisk(sidecarPath);
			if (!diskPath || !fs::exists(*diskPath)) return;

			toml::table root;
			try
			{
				root = toml::parse_file(diskPath->string());
			}
			catch (const toml::parse_error& e)
			{
				KOI_LOG_WARN("Mesh sidecar '{}' did not parse: {}", diskPath->string(), e.description());
				return;
			}

			const toml::array* slots = root["slot"].as_array();
			if (!slots) return;

			for (const toml::node& node : *slots)
			{
				const toml::table* slot = node.as_table();
				if (!slot) continue;

				const std::optional<u32>         index = (*slot)["index"].value<u32>();
				const std::optional<std::string> key   = (*slot)["material"].value<std::string>();
				if (!index || !key) continue;

				if (*index >= model.MaterialBindings.size()) model.MaterialBindings.resize(*index + 1);
				model.MaterialBindings[*index] = AssetKey(*key);
			}

			KOI_LOG_DEBUG("Mesh sidecar '{}': {} slot(s)", sidecarPath, model.MaterialBindings.size());
		}
	}

	std::unique_ptr<ModelAsset> AssetTraits<GPUMesh>::Load(const std::string& vfsPath)
	{
		std::unique_ptr<ModelAsset> model = MeshLoader::Load(vfsPath);
		if (model && model->Meshes.empty())
			return nullptr;

		if (model) ReadMeshSidecar(vfsPath, *model);

		return model;
	}

	GPUMesh AssetTraits<GPUMesh>::Upload(GfxDevice& device, const ModelAsset& source)
	{
		GPUMesh mesh = GPUMesh::Upload(device, source);
		mesh.BindMaterials(device, source);
		return mesh;
	}

	void AssetTraits<GPUMesh>::Destroy(GfxDevice& device, GPUMesh& resource)
	{
		resource.Release(device);
	}
}
