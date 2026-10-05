#include "Koi/Engine/Graphics/RenderPasses/GridPass.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Graphics/Camera.h"
#include "Koi/Engine/Graphics/RendererResources.h"
#include "Koi/Engine/Graphics/RHI/CommandList.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include <cassert>

namespace koi
{
	GridPass::GridPass(const Config& config)
		: m_config(config)
	{
		ZoneScoped;
		assert(m_config.Device && "GridPass requires a valid GfxDevice");

		m_constants.Init(*m_config.Device, BufferUsage::Constant, 1, "Grid Constants");
	}

	GridPass::~GridPass() = default;

	void GridPass::Reset()
	{
		m_camera      = nullptr;
		m_colorTarget = {};
		m_depth       = {};
		m_width       = 0;
		m_height      = 0;
	}

	GridPass& GridPass::SetTarget(TextureHandle colorTarget, TextureHandle depth, u32 width, u32 height)
	{
		m_colorTarget = colorTarget;
		m_depth       = depth;
		m_width       = width;
		m_height      = height;
		return *this;
	}

	void GridPass::Execute(CommandList& cmd)
	{
		ZoneScoped;
		if (!m_settings.Enabled || !m_colorTarget || !m_depth || !m_camera) return;
		if (!m_config.Resources || !m_config.Resources->GridPipeline) return;

		GfxDevice& device   = *m_config.Device;
		const u32  frameSlot = static_cast<u32>(device.GetCurrentFrameIndex() % FRAMES_IN_FLIGHT);

		const DescriptorIndex constantsIndex = m_constants.Upload(frameSlot, GridConstants
		{
			.ViewProjection    = m_camera->ViewProjection,
			.InvViewProjection = m_camera->ViewProjection.Invert(),
			.CameraPosition    = Vector4(m_camera->Position.x, m_camera->Position.y, m_camera->Position.z, 0.0f),
			.ThinColor         = m_settings.ThinColor,
			.ThickColor        = m_settings.ThickColor,
			.AxisXColor        = m_settings.AxisXColor,
			.AxisZColor        = m_settings.AxisZColor,
			.CellSize          = m_settings.CellSize,
			.ThickEvery        = m_settings.ThickEvery,
			.FadeStart         = m_settings.FadeStart,
			.FadeEnd           = m_settings.FadeEnd,
			.MajorLineWidth    = m_settings.MajorLineWidth,
		});
		if (constantsIndex == INVALID_DESCRIPTOR) return;

		cmd.TransitionBarrier(m_colorTarget, D3D12_RESOURCE_STATE_RENDER_TARGET);
		cmd.TransitionBarrier(m_depth, D3D12_RESOURCE_STATE_DEPTH_WRITE);

		cmd.SetRenderTarget(m_colorTarget, m_depth);
		cmd.SetViewportAndScissor(m_width, m_height);
		cmd.SetPipeline(m_config.Resources->GridPipeline);

		cmd.SetRootConstants(&constantsIndex, 1);
		cmd.Draw(3);
	}
}
