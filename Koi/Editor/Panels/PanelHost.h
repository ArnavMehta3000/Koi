#pragma once
#include "Koi/Engine/Core/Types.h"
#include "Koi/Engine/Interop/EditorPanel.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace koi::editor
{
	class PanelHost : public NonCopyable, public NonMovable
	{
	public:
		void Add(std::unique_ptr<IEditorPanel> panel);

		template<typename T, typename... Args>
		T& Emplace(Args&&... args)
		{
			auto panel = std::make_unique<T>(std::forward<Args>(args)...);
			T& ref = *panel;
			Add(std::move(panel));
			return ref;
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
		std::vector<std::unique_ptr<IEditorPanel>> m_panels;
		std::string                                m_title;
	};
}
