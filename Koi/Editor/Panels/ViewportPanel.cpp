#include "Koi/Editor/Panels/ViewportPanel.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Graphics/RenderWindow.h"
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

	void ViewportPanel::DrawToolbar()
	{
		static constexpr const char* kCameraLabels[]     = { "Scene", "Game" };
		static constexpr const char* kResolutionLabels[] = { "Free Aspect", "Window", "1920x1080", "1280x720" };

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 2.0f));

		ImGui::SetNextItemWidth(90.0f);
		int cameraMode = static_cast<int>(m_cameraMode);
		if (ImGui::Combo("##camera", &cameraMode, kCameraLabels, IM_ARRAYSIZE(kCameraLabels)))
			m_cameraMode = static_cast<ViewportCamera>(cameraMode);

		// Resolution only means something for the game camera; the scene camera
		// always fills the panel
		if (m_cameraMode == ViewportCamera::Game)
		{
			ImGui::SameLine();
			ImGui::SetNextItemWidth(120.0f);
			int resolution = static_cast<int>(m_resolution);
			if (ImGui::Combo("##resolution", &resolution, kResolutionLabels, IM_ARRAYSIZE(kResolutionLabels)))
				m_resolution = static_cast<ViewportResolution>(resolution);
		}

		ImGui::PopStyleVar();
	}

	void ViewportPanel::OnDraw()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		if (ImGui::Begin("Viewport"))
		{
			DrawToolbar();

			// Nothing to size a target from when collapsed, and the aspect maths
			// below would divide by zero
			const ImVec2 avail = ImGui::GetContentRegionAvail();
			if (avail.x >= 1.0f && avail.y >= 1.0f)
			{
				// Scene view always tracks the panel; the fixed modes render at their
				// own size and get scaled to fit below
				switch (m_cameraMode == ViewportCamera::Scene ? ViewportResolution::FreeAspect : m_resolution)
				{
					case ViewportResolution::Window:
						if (const RenderWindow* rw = m_renderer->GetMainRenderWindow())
						{
							m_desiredWidth  = rw->GetWidth();
							m_desiredHeight = rw->GetHeight();
							break;
						}
						[[fallthrough]];  // no window yet, fall back to the panel

					case ViewportResolution::FreeAspect:
						m_desiredWidth  = static_cast<u32>(avail.x);
						m_desiredHeight = static_cast<u32>(avail.y);
						break;

					case ViewportResolution::R1920x1080: m_desiredWidth = 1920; m_desiredHeight = 1080; break;
					case ViewportResolution::R1280x720:  m_desiredWidth = 1280; m_desiredHeight = 720;  break;
				}

				if (m_valid)
				{
					GfxDevice& device = m_renderer->GetDevice();
					if (const Texture* tex = device.Get(m_target.ResolvedColorBuffer);
						tex && tex->SRVIndex != INVALID_DESCRIPTOR)
					{
						// Fit the target inside the panel without distorting it; when the
						// aspects already agree this is the full region
						const f32 targetAspect = static_cast<f32>(m_target.Width) / static_cast<f32>(m_target.Height);
						const f32 availAspect  = avail.x / avail.y;

						const ImVec2 size = (targetAspect > availAspect)
							? ImVec2(avail.x, avail.x / targetAspect)
							: ImVec2(avail.y * targetAspect, avail.y);

						const ImVec2 cursor = ImGui::GetCursorPos();
						ImGui::SetCursorPos(ImVec2(cursor.x + (avail.x - size.x) * 0.5f,
						                           cursor.y + (avail.y - size.y) * 0.5f));

						const auto gpu = device.ResourceHeap().GpuHandle(tex->SRVIndex);
						ImGui::Image(static_cast<ImTextureID>(gpu.ptr), size);

						m_isHovered = ImGui::IsItemHovered() || ImGui::IsItemActive();
					}
				}
			}
			else
			{
				m_isHovered = false;
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
		const Vector3 center = bounds.Center();
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
