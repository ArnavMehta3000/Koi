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

	private:
		void CreateRenderTargets();
		void DestroyRenderTargets();

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

		std::array<RenderPassAttachment, size_t(Outputs::MAX_COUNT)> m_outputs;
	};
}
