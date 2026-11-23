#include "Koi/Editor/Panels/ViewportPanel.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/RHI/Device.h"
#include "Koi/Engine/Platform/InputManager.h"
#include "Koi/Engine/Runtime/Engine.h"

namespace koi::editor
{
	static constexpr Format kResolvedFormat = Format::R8G8B8A8_UNORM;

	ViewportPanel::ViewportPanel()
		: m_renderer(&Engine::Get().GetSubsystem<Renderer>())
	{
	}

	ViewportPanel::~ViewportPanel()
	{
		if (m_target.ResolvedColorBuffer) m_renderer->GetDevice().Destroy(m_target.ResolvedColorBuffer);
	}

	void ViewportPanel::EnsureSize(u32 width, u32 height)
	{
		ZoneScoped;
		if (width == 0 || height == 0)
		{
			m_valid = false;
			return;
		}

		if (m_valid && m_target.Width == width && m_target.Height == height) return;

		GfxDevice& device = m_renderer->GetDevice();
		if (m_target.ResolvedColorBuffer) device.Destroy(m_target.ResolvedColorBuffer);

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

		OnTargetResized(width, height);
	}

	const ViewportTarget* ViewportPanel::PrepareFrame()
	{
		// Resizes the target to what the last draw asked for. Null while there's nothing to render into
		ZoneScoped;
		EnsureSize(m_desiredWidth, m_desiredHeight);
		return GetTarget();
	}

	void ViewportPanel::ChooseSize(const ImVec2& avail, u32& outWidth, u32& outHeight)
	{
		outWidth  = static_cast<u32>(avail.x);
		outHeight = static_cast<u32>(avail.y);
	}

	void ViewportPanel::UpdateCapture(const InputManager& input, bool blocked)
	{
		const bool anyMouseDown = input.GetMouse().IsButtonDown(MouseButton::Left)
			|| input.GetMouse().IsButtonDown(MouseButton::Right)
			|| input.GetMouse().IsButtonDown(MouseButton::Middle);

		if (m_isHovered && anyMouseDown && !blocked)
			m_isCapturing = true;
		else if (!anyMouseDown)
			m_isCapturing = false;
	}

	void ViewportPanel::OnUpdate()
	{
		m_isHovered = false;  // OnDraw sets it while the image is on screen
	}

	void ViewportPanel::OnDraw()
	{
		DrawToolbar();

		// Nothing to size a target from when collapsed, and the aspect maths below would divide by zero
		const ImVec2 avail = ImGui::GetContentRegionAvail();
		if (avail.x >= 1.0f && avail.y >= 1.0f)
		{
			ChooseSize(avail, m_desiredWidth, m_desiredHeight);

			GfxDevice& device = m_renderer->GetDevice();
			const Texture* tex = m_valid ? device.Get(m_target.ResolvedColorBuffer) : nullptr;
			if (tex && tex->SRVIndex != INVALID_DESCRIPTOR)
			{
				m_renderer->RequestShaderRead(m_target.ResolvedColorBuffer);

				// Fit the target inside the panel without distorting it
				const f32 targetAspect = static_cast<f32>(m_target.Width) / static_cast<f32>(m_target.Height);
				const f32 availAspect  = avail.x / avail.y;

				const ImVec2 size = (targetAspect > availAspect)
					? ImVec2(avail.x, avail.x / targetAspect)
					: ImVec2(avail.y * targetAspect, avail.y);

				const ImVec2 cursor = ImGui::GetCursorPos();
				ImGui::SetCursorPos(ImVec2(cursor.x + (avail.x - size.x) * 0.5f,
				                           cursor.y + (avail.y - size.y) * 0.5f));

				const ImVec2 imageMin = ImGui::GetCursorScreenPos();
				const auto gpu = device.ResourceHeap().GpuHandle(tex->SRVIndex);
				ImGui::Image(static_cast<ImTextureID>(gpu.ptr), size);

				m_isHovered = ImGui::IsItemHovered() || ImGui::IsItemActive();

				OnImageGui(imageMin, size);
			}
		}

		OnWindowGui();
	}
}
