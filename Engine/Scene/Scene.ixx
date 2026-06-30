module;
#include "Core/Config.h"
#include <string>
#include <span>
#include <optional>
#include <filesystem>
#include <vector>
export module Koi.Scene;

import Koi.Core.Types;
import Koi.Core.Math;
import Koi.Core.Transform;
import Koi.Graphics.Types;
import Koi.Graphics.GPUMesh;

export namespace koi
{
	class GfxDevice;

	struct SceneNode
	{
		std::string    Name;
		Transform      LocalTransform;
		math::float4x4 WorldTransform = math::float4x4::identity();
		i32            Parent    = -1;
		i32            MeshIndex = -1;
		std::vector<i32> Children;
		bool           Dirty     = true;
	};

	class Scene
	{
	public:
		Scene() = default;
		~Scene() = default;
		Scene(const Scene&)            = delete;
		Scene& operator=(const Scene&) = delete;
		Scene(Scene&&) noexcept            = default;
		Scene& operator=(Scene&&) noexcept = default;

		[[nodiscard]] static std::optional<Scene> Load(const std::filesystem::path& path,
		                                               GfxDevice& device,
		                                               DescriptorIndex fallbackAlbedoSRV);

		void Update();
		void Release(GfxDevice& device);

#if KOI_WITH_EDITOR
		void DrawImGui();
#endif

		[[nodiscard]] std::span<const SceneNode> GetNodes()  const        { return m_nodes;         }
		[[nodiscard]] std::span<SceneNode>       GetNodes()               { return m_nodes;         }
		[[nodiscard]] const GPUMesh&             GetMesh(i32 index) const { return m_meshes[index]; }
		[[nodiscard]] const math::AABB&          GetBounds() const        { return m_bounds;        }

	private:
		std::vector<SceneNode> m_nodes;
		std::vector<GPUMesh>   m_meshes;
		math::AABB             m_bounds;
	};
}
