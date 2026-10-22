#pragma once
#include "Koi/Editor/Core/EditorPanel.h"
#include "Koi/Engine/Core/Types.h"
#include <concepts>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace koi::editor
{
	class EditorContext;

	class PanelHost : public NonCopyable, public NonMovable
	{
	public:
		explicit PanelHost(EditorContext* ctx) : m_ctx(ctx) {}

		void Add(std::unique_ptr<IEditorPanel> panel);

		// Constructed as T(EditorContext*, args...) when it takes one, otherwise T(args...)
		template<typename T, typename... Args>
		T* Emplace(Args&&... args)
		{
			std::unique_ptr<T> panel;
			if constexpr (std::constructible_from<T, EditorContext*, Args...>)
			{
				panel = std::make_unique<T>(m_ctx, std::forward<Args>(args)...);
			}
			else
			{
				panel = std::make_unique<T>(std::forward<Args>(args)...);
			}

			T* raw = panel.get();
			Add(std::move(panel));
			return raw;
		}

		template<typename T>
		[[nodiscard]] T* Find() const
		{
			for (const std::unique_ptr<IEditorPanel>& panel : m_panels)
			{
				if (T* typed = dynamic_cast<T*>(panel.get())) return typed;
			}
			return nullptr;
		}

		// Open/closed state lives in imgui.ini; call once the ImGui context exists
		void RegisterSettingsHandler();

		void DrawAll();
		void DrawMenuItems();

	private:
		[[nodiscard]] IEditorPanel* Find(std::string_view name) const;

		// "<icon>  Name###Name": the ### keeps the window ID (and saved layouts) independent of the icon
		[[nodiscard]] const char* Title(const IEditorPanel& panel, bool withId);

	private:
		EditorContext*                             m_ctx = nullptr;
		std::vector<std::unique_ptr<IEditorPanel>> m_panels;
		std::string                                m_title;
	};
}
