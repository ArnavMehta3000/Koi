#pragma once
#include "Koi/Engine/Graphics/RenderPasses/RenderPass.h"
#include <array>

namespace koi
{
	class  World;
	class  RenderWindow;
	struct Camera;
	struct RendererResources;

	class GBufferPass : public RenderPass
	{
	public:
		struct Desc
		{
			World*                   World        = nullptr;
			const Camera*            Camera       = nullptr;
			const RendererResources* Resources    = nullptr;
			RenderWindow*            RenderWindow = nullptr;
			TextureHandle            DepthBuffer;
		};

		explicit GBufferPass(const Desc& desc);

		std::string_view GetName() const override { return "GBuffer"; }
		std::span<const RenderPassAttachment> GetOutputs() const override { return m_outputs; }
		void Execute(CommandList& cmd) override;

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
		Desc m_desc;
		std::array<RenderPassAttachment, size_t(Outputs::MAX_COUNT)> m_outputs;
	};
}
