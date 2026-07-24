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
			u32                      Width     = 0;
			u32                      Height    = 0;
			GfxDevice*               Device    = nullptr;
			const RendererResources* Resources = nullptr;
		};

		explicit GBufferPass(const Config& config);

		std::string_view GetName() const override { return "GBuffer"; }
		std::span<const RenderPassAttachment> GetOutputs() const override { return m_outputs; }
		void Execute(CommandList& cmd) override;
		void Reset() override;

		inline void SetWorld(World& world) { m_world = &world; }
		inline void SetCamera(const Camera& camera) { m_camera = &camera; }
		void SetRTSize(u32 width, u32 height);

	private:
		void CreateRenderTargets();

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

		std::array<RenderPassAttachment, size_t(Outputs::MAX_COUNT)> m_outputs;
	};
}
