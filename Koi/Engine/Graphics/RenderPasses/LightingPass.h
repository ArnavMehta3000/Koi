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
	struct RendererResources;

	class LightingPass : public RenderPass
	{
	public:
		struct Config
		{
			GfxDevice*               Device    = nullptr;
			const RendererResources* Resources = nullptr;
		};

		explicit LightingPass(const Config& config);
		~LightingPass() override;

		std::string_view GetName() const override { return "Lighting"; }
		std::span<const RenderPassAttachment> GetInputs() const override { return m_inputs; }
		std::span<const RenderPassAttachment> GetOutputs() const override { return { &m_output, 1 }; }
		void Reset() override;
		void Execute(CommandList& cmd) override;

		inline LightingPass& SetWorld(World* world) { m_world = world; return *this; }
		inline LightingPass& SetCamera(const Camera* camera) { m_camera = camera; return *this; }

		// G-buffer inputs (names must match GBufferPass outputs for graph validation).
		LightingPass& SetGBuffer(TextureHandle albedo, TextureHandle normals, TextureHandle material, TextureHandle depth);

		// Null handle disables shadowing for this frame
		LightingPass& SetShadow(TextureHandle shadowMap, const Matrix& lightViewProjection);
		LightingPass& SetTarget(TextureHandle colorTarget, u32 width, u32 height);

		[[nodiscard]] inline TextureHandle GetColorOutput() const { return m_colorTarget; }
		[[nodiscard]] inline u32 GetLightCount() const { return m_lightCount; }

	private:
		void ConfigureECS();

		enum class Input { Albedo, Normals, Material, Depth, Shadow, COUNT };

		Config        m_config;
		World*        m_world  = nullptr;
		const Camera* m_camera = nullptr;

		std::array<RenderPassAttachment, size_t(Input::COUNT)> m_inputs;
		RenderPassAttachment m_output{ {}, "HDRColor" };  // the HDR buffer this pass writes
		TextureHandle m_colorTarget;
		u32           m_width  = 0;
		u32           m_height = 0;

		GpuFrameBuffer<GPULight> m_lightBuffer;     // StructuredBuffer<GPULight>, grown on demand
		u32                      m_lightCount = 0;  // lights uploaded this frame

		Matrix          m_lightViewProjection = Matrix::Identity;
		DescriptorIndex m_shadowMapIndex      = INVALID_DESCRIPTOR;

		std::vector<GPULight>    m_lights;

		flecs::world_t* m_queryWorld = nullptr;
		flecs::system   m_gatherSystem;
	};
}
