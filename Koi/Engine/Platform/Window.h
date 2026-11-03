#pragma once
#include "Koi/Engine/Core/Types.h"
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

struct HWND__;

namespace koi
{
	enum class WindowMode : u8
	{
		Windowed,
		Borderless,
		Fullscreen,  // borderless, covering the monitor
	};

	struct WindowDesc
	{
		std::string Title  = "Koi Window";
		WindowMode Mode    = WindowMode::Windowed;
		u32 Width          = 1280;
		u32 Height         = 720;
		bool Resizable     = true;
		bool VSync         = true;
	};

	// A Win32 message as the window procedure got it
	struct WindowMessage
	{
		HWND__* Hwnd;
		u32     Msg;
		u64     WParam;
		i64     LParam;
	};

	class Window : NonCopyable, NonMovable
	{
	public:
		using ResizeCallback = std::function<void(u32, u32)>;
		using CloseCallback  = std::function<void()>;
		using DropCallback   = std::function<void(std::span<const std::filesystem::path>)>;
		using MessageHook    = std::function<std::optional<i64>(const WindowMessage&)>;  // a value marks the message handled

	public:
		explicit Window(const WindowDesc& desc);
		~Window();

		[[nodiscard]] bool ShouldClose() const noexcept { return m_shouldClose; }
		void RequestClose() noexcept { m_shouldClose = true; }

		void SetMode(WindowMode newMode);
		void SetTitle(std::string_view title);
		void Minimize();
		void Maximize();
		void Restore();
		void Focus();

		[[nodiscard]] bool IsFocused() const noexcept;
		[[nodiscard]] bool IsHovered() const noexcept;
		[[nodiscard]] bool IsVisible() const noexcept;
		[[nodiscard]] bool IsMinimized() const noexcept;
		[[nodiscard]] bool IsMaximized() const noexcept;
		void SetSize(u32 width, u32 height);

		inline void SetResizeCallback(ResizeCallback cb) { m_onResize = std::move(cb); }
		inline void SetCloseCallback(CloseCallback cb) { m_onClose = std::move(cb); }
		inline void SetDropCallback(DropCallback cb) { m_onDrop = std::move(cb); }  // files dragged in from Explorer

		// Hooks see every message before the window does, in the order they were added
		u32  AddMessageHook(MessageHook hook);
		void RemoveMessageHook(u32 id);

		[[nodiscard]] inline WindowMode GetMode() const noexcept { return m_mode; }
		[[nodiscard]] inline void* GetNativeHandle() const noexcept { return m_hwnd; }
		[[nodiscard]] inline u32 GetWidth() const noexcept { return m_width; }
		[[nodiscard]] inline u32 GetHeight() const noexcept { return m_height; }
		[[nodiscard]] inline f32 AspectRatio() const noexcept { return static_cast<f32>(m_width) / static_cast<f32>(m_height); }

		// The DPI scale of the monitor the window is on, 1 at 96 DPI
		[[nodiscard]] f32 GetContentScale() const noexcept;

		static void PollEvents() noexcept;

	private:
		void Create(const WindowDesc& desc);
		void Destroy();
		void ApplyMode(WindowMode mode);

		// WindowProc.cpp
		static i64 __stdcall WndProc(HWND__* hwnd, u32 msg, u64 wParam, i64 lParam) noexcept;
		i64 HandleMessage(u32 msg, u64 wParam, i64 lParam);
		i64 HitTest(i32 x, i32 y) const noexcept;

	private:
		struct Hook { u32 Id; MessageHook Fn; };

		HWND__*        m_hwnd        = nullptr;
		u32            m_width       = 0;
		u32            m_height      = 0;
		WindowMode     m_mode        = WindowMode::Windowed;
		bool           m_resizable   = true;
		bool           m_shouldClose = false;
		std::string    m_title;

		// Outer rect to return to when leaving a fullscreen mode
		i32 m_savedX = 0;
		i32 m_savedY = 0;
		i32 m_savedW = 0;
		i32 m_savedH = 0;

		ResizeCallback    m_onResize;
		CloseCallback     m_onClose;
		DropCallback      m_onDrop;
		std::vector<Hook> m_hooks;
		u32               m_nextHookId = 1;

		static inline u32 s_windowCount = 0;
	};
}
