#pragma once
#include "Koi/Editor/Core/EditorPanel.h"
#include "Koi/Editor/Inspectors/ComponentInspector.h"
#include <memory>
#include <utility>

namespace koi
{
	class AppBase;
	class IReloadable;
}

namespace koi::editor
{
	class IEditorContext
	{
	public:
		virtual ~IEditorContext() = default;

		[[nodiscard]] virtual AppBase* GetGame() const = 0;

		virtual void RegisterReloadable(IReloadable* reloadable) = 0;
		virtual void RegisterPanel(std::unique_ptr<IEditorPanel> panel) = 0;
		virtual void RegisterInspector(std::unique_ptr<IComponentInspector> inspector) = 0;

		template<typename T, typename... Args>
		T& RegisterPanel(Args&&... args)
		{
			auto panel = std::make_unique<T>(std::forward<Args>(args)...);
			T& ref = *panel;
			RegisterPanel(std::move(panel));
			return ref;
		}

		template<typename T, typename... Args>
		T& RegisterInspector(Args&&... args)
		{
			auto inspector = std::make_unique<T>(std::forward<Args>(args)...);
			T& ref = *inspector;
			RegisterInspector(std::move(inspector));
			return ref;
		}
	};
}
