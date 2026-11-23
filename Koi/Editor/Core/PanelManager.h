#pragma once
#include "Koi/Editor/Core/Panel.h"
#include "Koi/Engine/Core/Types.h"
#include <concepts>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace koi::editor
{
	class EditorContext;

	template <typename T>
	concept PanelType = std::derived_from<T, Panel> && requires { { T::kInfo } -> std::convertible_to<const PanelInfo&>; };

	class PanelManager : public NonCopyable, public NonMovable
	{
	public:
		explicit PanelManager(EditorContext* ctx) : m_ctx(ctx) {}

		// Constructed as T(EditorContext*, args...) when it takes one, otherwise T(args...). One per name; null for a duplicate
		template <PanelType T, typename... Args>
		T* Add(Args&&... args)
		{
			std::unique_ptr<T> panel;
			if constexpr (std::constructible_from<T, EditorContext*, Args...>)
				panel = std::make_unique<T>(m_ctx, std::forward<Args>(args)...);
			else
				panel = std::make_unique<T>(std::forward<Args>(args)...);

			return static_cast<T*>(Add(std::move(panel), T::kInfo));
		}

		// A panel that is only a draw function, for small debug UI
		void Add(std::string name, const char* icon, std::function<void()> draw);

		template <PanelType T>
		[[nodiscard]] T* Find() const
		{
			for (const std::unique_ptr<Panel>& panel : m_panels)
			{
				if (panel->m_info == &T::kInfo) return static_cast<T*>(panel.get());
			}
			return nullptr;
		}

		// Open/closed state lives in imgui.ini; call once the ImGui context exists
		void RegisterSettingsHandler();

		// Each panel's section under [panels] in Editor.toml
		void SaveState(toml::table& panels) const;
		void LoadState(const toml::table& panels);

		void TickAll(f32 dt);
		void RenderAll();
		void DrawAll();
		void DrawMenuItems();

	private:
		Panel* Add(std::unique_ptr<Panel> panel, const PanelInfo& info);
		[[nodiscard]] Panel* Find(std::string_view name) const;

		// "<icon>  Name###Name": the ### keeps the window ID (and saved layouts) independent of the icon
		[[nodiscard]] const char* Title(const Panel& panel, bool withId);

	private:
		EditorContext*                     m_ctx = nullptr;
		std::vector<std::unique_ptr<Panel>> m_panels;
		std::string                        m_title;
	};
}
