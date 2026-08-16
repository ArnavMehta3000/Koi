#pragma once
#include "Koi/Engine/Graphics/RenderPasses/RenderPass.h"
#include "Koi/Engine/Graphics/GpuFrameBuffer.h"
#include "Koi/Engine/Graphics/GPUSceneTypes.h"
#include <flecs.h>
#include <array>

namespace koi
{
	class  World;
	class  GfxDevice;
	struct Camera;
	struct GPUMesh;
	struct RendererResources;

	class GBufferPass : public RenderPass
	{
	public:
		struct Config
		{
			GfxDevice*               Device    = nullptr;
			const RendererResources* Resources = nullptr;
		};

		explicit GBufferPass(const Config& config);
		~GBufferPass();

		std::string_view GetName() const override { return "GBuffer"; }
		std::span<const RenderPassAttachment> GetOutputs() const override { return m_outputs; }
		void Execute(CommandList& cmd) override;
		void Reset() override;

		inline GBufferPass& SetWorld(World* world) { m_world = world; return *this; }
		inline GBufferPass& SetCamera(const Camera* camera) { m_camera = camera; return *this; }
		GBufferPass& SetTarget(u32 width, u32 height);

		[[nodiscard]] inline TextureHandle GetAlbedo()   const { return m_outputs[size_t(Outputs::Albedo)].Texture;   }
		[[nodiscard]] inline TextureHandle GetNormals()  const { return m_outputs[size_t(Outputs::Normals)].Texture;  }
		[[nodiscard]] inline TextureHandle GetMaterial() const { return m_outputs[size_t(Outputs::Material)].Texture; }
		[[nodiscard]] inline TextureHandle GetDepth()    const { return m_outputs[size_t(Outputs::Depth)].Texture;    }

	private:
		void CreateRenderTargets();
		void DestroyRenderTargets();
		void ConfigureECS();

	private:
		struct DrawInfo
		{
			const GPUMesh* Mesh;
			u32            ObjectIndex;
		};

		struct GatherContext
		{
			std::vector<PerObjectData> Objects;  // World + Normal Matrix
			std::vector<DrawInfo>      DrawInfos;

			void Clear() { Objects.clear(); DrawInfos.clear(); }
		};

	private:
		enum class Outputs
		{
			Albedo,    // RGB + AO (RGBA8_UNORM)
			Normals,   // XYZ + MatID (RGBA16_UNORM)
			Material,  // Roughness/Metallic/Specular (in rgb) + A (unused) (RGBA8_UNORM)
			Depth,     // DSV (D32)
			//TODO Emissive
			MAX_COUNT
		};

	private:
		Config        m_config;
		World*        m_world  = nullptr;
		const Camera* m_camera = nullptr;
		u32           m_width  = 0;
		u32           m_height = 0;

		flecs::world_t* m_queryWorld = nullptr;
		flecs::system   m_renderSystem;

		GpuFrameBuffer<FrameConstants> m_frameConstants;
		GpuFrameBuffer<PerObjectData>  m_objects;
		GatherContext                  m_gather;

		std::array<RenderPassAttachment, size_t(Outputs::MAX_COUNT)> m_outputs;
	};
}
