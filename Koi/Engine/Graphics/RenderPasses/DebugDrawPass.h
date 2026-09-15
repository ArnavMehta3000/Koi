#pragma once
#include "Koi/Engine/Graphics/RenderPasses/RenderPass.h"
#include "Koi/Engine/Graphics/DebugDraw.h"
#include "Koi/Engine/Graphics/GpuFrameBuffer.h"

namespace koi
{
	class  GfxDevice;
	struct Camera;
	struct RendererResources;

	class DebugDrawPass : public RenderPass
	{
	public:
		struct Config
		{
			GfxDevice*               Device    = nullptr;
			const RendererResources* Resources = nullptr;
			const DebugDrawQueue*    Queue     = nullptr;
		};

		explicit DebugDrawPass(const Config& config);
		~DebugDrawPass() override;

		inline std::string_view GetName() const override { return "Debug Draw"; }
		void Reset() override;
		void Execute(CommandList& cmd) override;

		inline DebugDrawPass& SetCamera(const Camera* camera) { m_camera = camera; return *this; }
		DebugDrawPass& SetTarget(TextureHandle colorTarget, TextureHandle depth, u32 width, u32 height);

		[[nodiscard]] inline u32 GetLineCount() const { return m_lineCount; }

	private:
		Config                     m_config;
		const Camera*              m_camera = nullptr;

		TextureHandle              m_colorTarget;
		TextureHandle              m_depth;
		u32                        m_width  = 0;
		u32                        m_height = 0;

		GpuFrameBuffer<DebugVertex> m_vertexBuffer;
		u32                         m_lineCount = 0;
	};
}
