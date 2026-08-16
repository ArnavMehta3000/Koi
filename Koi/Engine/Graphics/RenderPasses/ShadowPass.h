#pragma once
#include "Koi/Engine/Graphics/RenderPasses/RenderPass.h"
#include "Koi/Engine/Graphics/GpuFrameBuffer.h"
#include "Koi/Engine/Graphics/GPUSceneTypes.h"
#include <flecs.h>

namespace koi
{
	class  World;
	class  GfxDevice;
	struct GPUMesh;
	struct RendererResources;

	class ShadowPass : public RenderPass
	{
	public:
		struct Config
		{
			GfxDevice*               Device     = nullptr;
			const RendererResources* Resources  = nullptr;
			u32                      Resolution = 2048;
		};

		explicit ShadowPass(const Config& config);
		~ShadowPass();

		std::string_view GetName() const override { return "Shadow"; }
		std::span<const RenderPassAttachment> GetOutputs() const override { return { &m_output, 1 }; }
		void Execute(CommandList& cmd) override;
		void Reset() override;

		// Also picks the caster and fits the light matrix, so both are readable before Execute
		ShadowPass& SetWorld(World* world);

		// Null handle when nothing casts a shadow this frame
		[[nodiscard]] inline TextureHandle GetShadowMap() const { return m_hasShadow ? m_output.Texture : TextureHandle(); }
		[[nodiscard]] inline const Matrix& GetLightViewProjection() const { return m_lightViewProjection; }

	private:
		void ConfigureECS();

		// Ortho light-space view-projection fitted to the world bounds. False when no caster
		bool ComputeLightMatrix();

	private:
		struct DrawInfo
		{
			const GPUMesh* Mesh;
			u32            ObjectIndex;
		};

		struct GatherContext
		{
			std::vector<PerObjectData> Objects;
			std::vector<DrawInfo>      DrawInfos;

			void Clear() { Objects.clear(); DrawInfos.clear(); }
		};

	private:
		Config m_config;
		World* m_world = nullptr;

		Matrix m_lightViewProjection = Matrix::Identity;
		bool   m_hasShadow           = false;

		flecs::world_t* m_queryWorld = nullptr;
		flecs::system   m_renderSystem;

		GpuFrameBuffer<FrameConstants> m_frameConstants;
		GpuFrameBuffer<PerObjectData>  m_objects;
		GatherContext                  m_gather;

		RenderPassAttachment m_output{ {}, "ShadowMap" };
	};
}
