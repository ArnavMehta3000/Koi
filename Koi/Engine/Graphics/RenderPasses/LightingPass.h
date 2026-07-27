#pragma once
#include "Koi/Engine/Graphics/RenderPasses/RenderPass.h"
#include <flecs.h>
#include <array>

namespace koi
{
	class  World;
	class  GfxDevice;
	struct Camera;
	struct RendererResources;

	// Full-screen deferred lighting. Each frame it (1) gathers every (Transform, Light)
	// entity into a ring-buffered StructuredBuffer<GPULight>, then (2) draws a full-screen
	// triangle that samples the G-buffer, reconstructs world position from depth, and
	// accumulates Blinn-Phong lighting into the HDR color target.
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
		std::span<const RenderPassAttachment> GetInputs() const override { return m_gbuffer; }
		std::span<const RenderPassAttachment> GetOutputs() const override { return { &m_output, 1 }; }
		void Reset() override;
		void Execute(CommandList& cmd) override;

		inline LightingPass& SetWorld(World* world) { m_world = world; return *this; }
		inline LightingPass& SetCamera(const Camera* camera) { m_camera = camera; return *this; }

		// G-buffer inputs (names must match GBufferPass outputs for graph validation).
		LightingPass& SetGBuffer(TextureHandle albedo, TextureHandle normals, TextureHandle material, TextureHandle depth);
		LightingPass& SetTarget(TextureHandle colorTarget, u32 width, u32 height);

		[[nodiscard]] inline TextureHandle GetColorOutput() const { return m_colorTarget; }
		[[nodiscard]] inline u32 GetLightCount() const { return m_lightCount; }

	private:
		void EnsureLightCapacity(u32 frameSlot, u32 requiredCount);
		void ConfigureECS();

		enum class GBuf { Albedo, Normals, Material, Depth, COUNT };

		Config        m_config;
		World*        m_world  = nullptr;
		const Camera* m_camera = nullptr;

		std::array<RenderPassAttachment, size_t(GBuf::COUNT)> m_gbuffer;
		RenderPassAttachment m_output{ {}, "HDRColor" };  // the HDR buffer this pass writes
		TextureHandle m_colorTarget;
		u32           m_width  = 0;
		u32           m_height = 0;

		FrameArray<BufferHandle> m_lightBuffers{};   // SRV, grown on demand
		FrameArray<u32>          m_lightCapacity{};   // element capacity of m_lightBuffers[i]
		u32                      m_lightCount = 0;    // lights uploaded this frame

		flecs::world_t* m_queryWorld = nullptr;
		flecs::system   m_gatherSystem;
	};
}
