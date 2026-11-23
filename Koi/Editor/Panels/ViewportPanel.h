#pragma once
#include "Koi/Editor/Core/Panel.h"
#include "Koi/Engine/Graphics/RenderView.h"

namespace koi
{
	class InputManager;
	class Renderer;
}

namespace koi::editor
{
	// A panel showing one offscreen target
	class ViewportPanel : public Panel
	{
	public:
		~ViewportPanel() override;

		void OnUpdate() override;
		void OnDraw() final;

		const ViewportTarget* PrepareFrame();

		[[nodiscard]] inline const ViewportTarget* GetTarget()   const { return m_valid ? &m_target : nullptr; }
		[[nodiscard]] inline bool                  IsHovered()   const { return m_isHovered; }
		[[nodiscard]] inline bool                  IsCapturing() const { return m_isCapturing; }

	protected:
		ViewportPanel();

		void UpdateCapture(const InputManager& input, bool blocked);

		virtual void DrawToolbar() {}
		virtual void ChooseSize(const ImVec2& avail, u32& outWidth, u32& outHeight);

		// Over the image, after it's drawn (in screen-space rect)
		virtual void OnImageGui([[maybe_unused]] const ImVec2& min, [[maybe_unused]] const ImVec2& size) {}
		virtual void OnWindowGui() {} // After the image (over the whole window)
		virtual void OnTargetResized([[maybe_unused]] u32 width, [[maybe_unused]] u32 height) {}

	protected:
		Renderer* m_renderer = nullptr;

	private:
		void EnsureSize(u32 width, u32 height);

	private:
		ViewportTarget m_target{};
		bool           m_valid         = false;
		u32            m_desiredWidth  = 1280;
		u32            m_desiredHeight = 720;
		bool           m_isHovered     = false;
		bool           m_isCapturing   = false;
	};
}
