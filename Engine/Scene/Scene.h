#pragma once
#include "Core/Math.h"
#include "Core/Transform.h"
#include "Graphics/GPUMesh.h"
#include "Graphics/Camera.h"
#include <string>
#include <span>
#include <optional>
#include <filesystem>

namespace koi
{
	class GfxDevice;
	namespace fs = std::filesystem;

	struct SceneNode
	{
		std::string    Name;
		Transform      LocalTransform;
		math::float4x4 WorldTransform = math::float4x4::identity();
		i32            Parent    = -1;
		i32            MeshIndex = -1;  // index into Scene meshes, -1 = transform-only node
		std::vector<i32> Children;
		bool           Dirty     = true;
	};

	class Scene
	{
	public:
		Scene() = default;
		~Scene() = default;
		Scene(const Scene&) = delete;
		Scene& operator=(const Scene&) = delete;
		Scene(Scene&&) noexcept = default;
		Scene& operator=(Scene&&) noexcept = default;

		[[nodiscard]] static std::optional<Scene> Load(const fs::path& path,
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

		[[nodiscard]] Camera&       GetCamera()       { return m_camera; }
		[[nodiscard]] const Camera& GetCamera() const { return m_camera; }

	private:
		std::vector<SceneNode> m_nodes;
		std::vector<GPUMesh>   m_meshes;
		math::AABB             m_bounds;
		Camera                 m_camera;
	};
}
