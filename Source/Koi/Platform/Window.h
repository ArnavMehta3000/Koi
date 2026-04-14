#pragma once
#include "Core/Core.h"
#include "Platform/Win32.h"
#include <string>

namespace koi
{
	struct WindowDesc
	{
		std::wstring Title = L"Koi Window";
		u32 Width          = 1280;
		u32 Height         = 720;
		bool Resizable     = true;
	};

	struct ResizeEvent
	{
		u32 Width;
		u32 Height;
	};

	class Window : NonCopyableNonMovable
	{
	public:
		explicit Window(const WindowDesc& desc);
		~Window();

		[[nodiscard]] bool ProcessMessages() const;

		[[nodiscard]] inline HWND GetHandle() const noexcept { return m_hWnd; }
		[[nodiscard]] inline u32 GetWidth() const noexcept { return m_width; }
		[[nodiscard]] inline u32 GetHeight() const noexcept { return m_height; }
		[[nodiscard]] inline bool IsAlive() const noexcept { return m_alive; }
		[[nodiscard]] inline bool IsFocused() const noexcept { return m_focused; }
		[[nodiscard]] inline f32 AspectRatio() const noexcept { return static_cast<f32>(m_width) / static_cast<f32>(m_height); }
		
		void SetTitle(const std::wstring& title) const;
		void Show(i32 showCmd = SW_SHOW) const;

	public:
		std::function<void(const ResizeEvent&)> OnResizeCallback;
		std::function<void()>                   OnCloseCallback;

	private:
		static LRESULT CALLBACK WndProcStatic(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
		LRESULT WndProc(UINT msg, WPARAM wp, LPARAM lp);

	private:
		HWND m_hWnd;
		u32  m_width;
		u32  m_height;
		bool m_alive = true;
		bool m_focused = false;
		bool m_resizing = false;

		// Input
		static inline HINSTANCE s_hInstance = nullptr;
		static inline u32       s_windowCount = 0;
		static constexpr auto   s_className = L"KoiWindowClass";
	};
}
