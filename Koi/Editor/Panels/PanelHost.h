#pragma once
#include "Koi/Engine/Core/Types.h"
#include "Koi/Engine/Interop/EditorPanel.h"
#include <memory>
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

	private:
		std::vector<std::unique_ptr<IEditorPanel>> m_panels;
	};
}
