module;
#include "Platform/Win32.h"
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <string>
#include <memory>
#include <functional>
export module Koi.Platform.Window;

import Koi.Core.Types;

export namespace koi
{
	enum class WindowMode : u8
	{
		Windowed,
		Borderless,
		WindowedFullscreen,
		ExclusiveFullscreen
	};

	struct WindowDesc
	{
		std::string Title  = "Koi Window";
		WindowMode  Mode   = WindowMode::Windowed;
		i32         MonitorID = 0;
		u32         Width  = 1280;
		u32         Height = 720;
		bool        Resizable = true;
		bool        VSync  = true;
	};

	class Window : NonCopyable
	{
	public:
		using ResizeCallback = std::function<void(u32, u32)>;
		using CloseCallback  = std::function<void()>;
		using FocusCallback  = std::function<void(bool)>;

	public:
		explicit Window(const WindowDesc& desc);
		~Window();

		Window(Window&& other) noexcept;
		Window& operator=(Window&& other) noexcept;

		[[nodiscard]] bool ShouldClose() const noexcept;
		void RequestClose();

		void SetMode(WindowMode newMode);
		void SetTitle(std::string_view title);
		void Minimize();
		void Maximize();
		void Restore();
		void Focus();

		[[nodiscard]] bool IsFocused()   const noexcept;
		[[nodiscard]] bool IsHovered()   const noexcept;
		[[nodiscard]] bool IsVisible()   const noexcept;
		[[nodiscard]] bool IsMinimized() const noexcept;
		[[nodiscard]] bool IsMaximized() const noexcept;
		void SetSize(u32 width, u32 height);

		inline void SetResizeCallback(ResizeCallback cb) { m_onResize = std::move(cb); }
		inline void SetCloseCallback(CloseCallback cb)   { m_onClose  = std::move(cb); }

		[[nodiscard]] inline WindowMode   GetMode()         const noexcept { return m_mode; }
		[[nodiscard]] inline void*        GetNativeHandle() const noexcept { return m_nativeHandle; }
		[[nodiscard]] inline GLFWwindow*  GetGLFWWindow()   const noexcept { return m_window; }
		[[nodiscard]] inline u32          GetWidth()        const noexcept { return m_width; }
		[[nodiscard]] inline u32          GetHeight()       const noexcept { return m_height; }
		[[nodiscard]] inline f32          AspectRatio()     const noexcept { return static_cast<f32>(m_width) / static_cast<f32>(m_height); }

		static void PollEvents() noexcept;

	private:
		void Create(const WindowDesc& desc);
		void Destroy();

		static void OnResize(GLFWwindow* w, i32 width, i32 height) noexcept;
		static void OnClose(GLFWwindow* w) noexcept;
		static void OnFocus(GLFWwindow* w, i32 focused) noexcept;

	private:
		GLFWwindow* m_window       = nullptr;
		void*       m_nativeHandle = nullptr;
		u32         m_width        = 0;
		u32         m_height       = 0;
		WindowMode  m_mode         = WindowMode::Windowed;
		std::string m_title;

		i32 m_savedX = 0, m_savedY = 0;
		i32 m_savedW = 0, m_savedH = 0;

		ResizeCallback m_onResize;
		CloseCallback  m_onClose;
		FocusCallback  m_onFocus;

		static inline u32 s_windowCount = 0;
	};
}
