#pragma once
#include "Editor/EditorPanel.h"
#include "Editor/ViewportTarget.h"
#include <memory>
#include <utility>

namespace koi
{
	class IReloadable;

	class IEditorContext
	{
	public:
		virtual ~IEditorContext() = default;

		virtual void RegisterReloadable(IReloadable* reloadable) = 0;
		virtual void RegisterPanel(std::unique_ptr<IEditorPanel> panel) = 0;

		[[nodiscard]] virtual const ViewportTarget* GetViewportTarget() const = 0;

		template<typename T, typename... Args>
		T& RegisterPanel(Args&&... args)
		{
			auto panel = std::make_unique<T>(std::forward<Args>(args)...);
			T& ref = *panel;
			RegisterPanel(std::move(panel));
			return ref;
		}
	};
}
