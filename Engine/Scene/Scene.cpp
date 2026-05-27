#include "Scene/Scene.h"
#include "Core/Logger.h"
#include "Core/Math.h"
#include "Graphics/RHI/Device.h"
#include "Graphics/MeshLoader.h"
#include "Memory/MemoryTracker.h"
#include <queue>
#include <unordered_map>
#include <cassert>
#include <numbers>
#include <toml++/toml.hpp>

namespace koi
{
	namespace
	{
		static constexpr f32 kDegToRad = static_cast<f32>(std::numbers::pi) / 180.0f;

		float ToFloat(const toml::array* arr, size_t i, f32 def)
		{
			if (!arr || i >= arr->size()) return def;
			const auto* v = arr->get_as<f64>(i);
			return v ? static_cast<f32>(v->get()) : def;
		}

		math::float3 ReadFloat3(const toml::array* arr, math::float3 def)
		{
			f32 x[3]{};
			math::store(x, def);
			return math::float3{ ToFloat(arr, 0, x[0]), ToFloat(arr, 1, x[1]), ToFloat(arr, 2, x[2]) };
		}
	}

	std::optional<Scene> Scene::Load(const fs::path& path, GfxDevice& device, DescriptorIndex fallbackAlbedoSRV)
	{
		KOI_MEM_TAG_SCOPE(MemTag::Assets);
		KOI_LOG_INFO("Loading scene: {}", path.string());

		Scene scene;
		scene.m_bounds.Min = math::float3(FLT_MAX, FLT_MAX, FLT_MAX);
		scene.m_bounds.Max = math::float3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

		try
		{
			auto toml = toml::parse_file(path.string());

			struct NodeDesc
			{
				std::string  name;
				std::string  parent;
				std::string  mesh;
				Transform    transform;
			};

			// Parse node descriptors
			std::vector<NodeDesc> descs;
			if (auto* nodes = toml.get_as<toml::array>("nodes"))
			{
				descs.reserve(nodes->size());
				for (auto& elem : *nodes)
				{
					auto* tbl = elem.as_table();
					if (!tbl) continue;

					NodeDesc d;
					d.name   = tbl->get_as<std::string>("name") ? **tbl->get_as<std::string>("name") : "";
					d.parent = tbl->get_as<std::string>("parent") ? **tbl->get_as<std::string>("parent") : "";
					d.mesh   = tbl->get_as<std::string>("mesh") ? **tbl->get_as<std::string>("mesh") : "";

					d.transform.Position    = ReadFloat3(tbl->get_as<toml::array>("position"), { 0.0f, 0.0f, 0.0f });
					d.transform.Scale       = ReadFloat3(tbl->get_as<toml::array>("scale"),    { 1.0f, 1.0f, 1.0f });

					// Rotation is authored in degrees, stored as radians
					math::float3 deg = ReadFloat3(tbl->get_as<toml::array>("rotation"), { 0.0f, 0.0f, 0.0f });
					f32 degArr[3]{};
					math::store(degArr, deg);
					d.transform.EulerAngles = math::float3
					{
						degArr[0] * kDegToRad,
						degArr[1] * kDegToRad,
						degArr[2] * kDegToRad,
					};

					descs.push_back(std::move(d));
				}
			}

			// Load unique meshes (deduplicated by path)
			std::unordered_map<std::string, i32> meshPathToIndex;
			for (const NodeDesc& d : descs)
			{
				if (d.mesh.empty()) continue;
				auto [it, inserted] = meshPathToIndex.try_emplace(d.mesh, (i32)scene.m_meshes.size());
				if (inserted)
				{
					auto model = MeshLoader::Load(d.mesh);
					assert(model && !model->Meshes.empty() && "Scene: failed to load mesh");
					scene.m_meshes.push_back(GPUMesh::Upload(device, *model, fallbackAlbedoSRV));
					scene.m_bounds.Expand(model->Bounds);
				}
			}

			// Build node list + name→index map
			std::vector<SceneNode> tempNodes;
			std::unordered_map<std::string, i32> nameToIdx;
			tempNodes.reserve(descs.size());

			for (const NodeDesc& d : descs)
			{
				i32 idx = (i32)tempNodes.size();
				if (!d.name.empty())
					nameToIdx.emplace(d.name, idx);

				SceneNode node;
				node.Name           = d.name;
				node.LocalTransform = d.transform;
				node.MeshIndex      = d.mesh.empty() ? -1 : meshPathToIndex.at(d.mesh);
				node.Dirty          = true;
				tempNodes.push_back(std::move(node));
			}

			// Link parent-child relationships
			for (i32 i = 0; i < (i32)descs.size(); ++i)
			{
				const std::string& parentName = descs[i].parent;
				if (parentName.empty()) continue;
				auto it = nameToIdx.find(parentName);
				assert(it != nameToIdx.end() && "Scene: parent node not found");
				i32 parentIdx = it->second;
				tempNodes[i].Parent = parentIdx;
				tempNodes[parentIdx].Children.push_back(i);
			}

			// BFS topological sort so parent index < child index.
			// This enables a single forward pass in Update().
			std::vector<i32> oldToNew(tempNodes.size(), -1);
			std::vector<SceneNode> sorted;
			sorted.reserve(tempNodes.size());

			std::queue<i32> q;
			for (i32 i = 0; i < (i32)tempNodes.size(); ++i)
			{
				if (tempNodes[i].Parent == -1)
					q.push(i);
			}

			while (!q.empty())
			{
				i32 old  = q.front(); q.pop();
				i32 next = (i32)sorted.size();
				oldToNew[old] = next;

				for (i32 child : tempNodes[old].Children)
				{
					q.push(child);
				}

				sorted.push_back(std::move(tempNodes[old]));
			}

			for (SceneNode& n : sorted)
			{
				if (n.Parent != -1)
					n.Parent = oldToNew[n.Parent];

				for (i32& c : n.Children)
				{
					c = oldToNew[c];
				}
			}

			scene.m_nodes = std::move(sorted);
			KOI_LOG_INFO("Scene loaded: {} nodes, {} meshes", scene.m_nodes.size(), scene.m_meshes.size());
			return scene;
		}
		catch (const std::exception& e)
		{
			KOI_LOG_ERROR("Failed to load scene '{}': {}", path.string(), e.what());
			return std::nullopt;
		}
	}

	void Scene::Update()
	{
		for (SceneNode& node : m_nodes)
		{
			if (!node.Dirty) continue;

			math::float4x4 local = node.LocalTransform.ToMatrix();

			node.WorldTransform = (node.Parent == -1)
				? local
				: local * m_nodes[node.Parent].WorldTransform;

			node.Dirty = false;

			// Propagate dirtiness to children
			for (i32 child : node.Children)
				m_nodes[child].Dirty = true;
		}
	}

	void Scene::Release(GfxDevice& device)
	{
		KOI_MEM_TAG_SCOPE(MemTag::Assets);
		for (GPUMesh& mesh : m_meshes)
			mesh.Release(device);
		m_meshes.clear();
		m_nodes.clear();
	}
}
