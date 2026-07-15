#include "Koi/Editor/Panels/ViewportPanel.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Graphics/RHI/CommandList.h"
#include "Koi/Engine/Math/Math.h"
#include "Koi/Engine/Platform/InputManager.h"
#include "Koi/Engine/Platform/InputCodes.h"

#include <imgui.h>

namespace koi
{
	// Must match the RT formats RendererResources bakes into MeshOpaquePipeline / TonemapPipeline
	static constexpr Format kHDRFormat = Format::R16G16B16A16_FLOAT;
	static constexpr Format kResolvedFormat = Format::R8G8B8A8_UNORM;

	ViewportPanel::ViewportPanel(Renderer& renderer)
		: m_renderer(&renderer)
		, m_controller(&m_camera, CameraMode::Orbit)
	{
	}

	ViewportPanel::~ViewportPanel()
	{
		GfxDevice& device = m_renderer->GetDevice();
		if (m_target.HDRColorBuffer)      device.Destroy(m_target.HDRColorBuffer);
		if (m_target.DepthBuffer)         device.Destroy(m_target.DepthBuffer);
		if (m_target.ResolvedColorBuffer) device.Destroy(m_target.ResolvedColorBuffer);
	}

	void ViewportPanel::EnsureSize(u32 width, u32 height)
	{
		if (width == 0 || height == 0)
		{
			m_valid = false;
			return;
		}

		if (m_valid && m_target.Width == width && m_target.Height == height) return;

		GfxDevice& device = m_renderer->GetDevice();
		if (m_target.HDRColorBuffer)      device.Destroy(m_target.HDRColorBuffer);
		if (m_target.DepthBuffer)         device.Destroy(m_target.DepthBuffer);
		if (m_target.ResolvedColorBuffer) device.Destroy(m_target.ResolvedColorBuffer);

		m_target.HDRColorBuffer = device.CreateTexture(TextureDesc{
			.Width  = width,
			.Height = height,
			.Format = kHDRFormat,
			.Usage  = TextureUsage::RenderTarget | TextureUsage::ShaderRead,
			.DebugName = "Viewport HDR Color",
		});

		const Format depthFormat = m_renderer->GetDepthFormat();
		m_target.DepthBuffer = (depthFormat != Format::UNKNOWN)
			? device.CreateTexture(TextureDesc
			{
				.Width     = width,
				.Height    = height,
				.Format    = depthFormat,
				.Usage     = TextureUsage::DepthStencil,
				.DebugName = "Viewport Depth",
			})
			: TextureHandle{};

		m_target.ResolvedColorBuffer = device.CreateTexture(TextureDesc
		{
			.Width     = width,
			.Height    = height,
			.Format    = kResolvedFormat,
			.Usage     = TextureUsage::RenderTarget | TextureUsage::ShaderRead,
			.DebugName = "Viewport Resolved Color",
		});

		m_target.Width  = width;
		m_target.Height = height;
		m_valid = true;

		m_camera.SetAspect(static_cast<f32>(width) / static_cast<f32>(height));
	}

	const ViewportTarget* ViewportPanel::PrepareFrame(CommandList& cmd)
	{
		EnsureSize(m_desiredWidth, m_desiredHeight);
		if (!m_valid) return nullptr;

		static constexpr f32 kClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		cmd.TransitionBarrier(m_target.HDRColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
		cmd.ClearRenderTarget(m_target.HDRColorBuffer, kClearColor);

		if (m_target.DepthBuffer)
		{
			cmd.TransitionBarrier(m_target.DepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE);
			cmd.ClearDepthStencil(m_target.DepthBuffer);
		}

		return &m_target;
	}

	void ViewportPanel::OnDraw()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		if (ImGui::Begin("Viewport"))
		{
			const ImVec2 avail = ImGui::GetContentRegionAvail();
			if (avail.x >= 1.0f && avail.y >= 1.0f)
			{
				m_desiredWidth  = static_cast<u32>(avail.x);
				m_desiredHeight = static_cast<u32>(avail.y);
			}

			if (m_valid)
			{
				GfxDevice& device = m_renderer->GetDevice();
				if (const Texture* tex = device.Get(m_target.ResolvedColorBuffer);
					tex && tex->SRVIndex != INVALID_DESCRIPTOR)
				{
					const auto gpu = device.ResourceHeap().GpuHandle(tex->SRVIndex);
					ImGui::Image(static_cast<ImTextureID>(gpu.ptr), avail);

					m_isHovered = ImGui::IsItemHovered() || ImGui::IsItemActive();
				}
			}
		}
		else
		{
			m_isHovered = false;
		}
		ImGui::End();
		ImGui::PopStyleVar();
	}

	void ViewportPanel::FrameBounds(const AABB& bounds)
	{
		const float3 center = bounds.Center();
		const f32 radius = bounds.Radius();

		m_camera.SetProjection(0.8f, m_camera.Aspect, 0.05f, 1000.0f);
		m_controller.SetOrbitTarget(center, radius * 2.5f, 0.0f, 0.3f);
	}

	void ViewportPanel::Update(f32 dt, const InputManager& input)
	{
		const bool anyMouseDown = input.IsMouseButtonDown(MouseButton::Left)
			|| input.IsMouseButtonDown(MouseButton::Right)
			|| input.IsMouseButtonDown(MouseButton::Middle);

		if (m_isHovered && anyMouseDown) m_isCapturingInput = true;
		else if (!anyMouseDown) m_isCapturingInput = false;

		if (m_isHovered || m_isCapturingInput)
		{
			m_controller.Update(dt, input);
		}
	}
}
