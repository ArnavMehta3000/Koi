#pragma once
#include "Koi/Engine/Interop/EditorPanel.h"
#include "Koi/Engine/Interop/ViewportTarget.h"
#include <memory>
#include <optional>
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

		[[nodiscard]] virtual std::optional<RenderView> GetEditorRenderView() const = 0;

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
