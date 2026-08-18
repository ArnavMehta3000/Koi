#pragma once
#include <string_view>

namespace koi
{
	class IEditorPanel
	{
	public:
		virtual ~IEditorPanel() = default;

		[[nodiscard]] virtual std::string_view GetName() const = 0;
		virtual void OnDraw() = 0;

		// Whether this panel can be toggled off
		[[nodiscard]] virtual bool IsClosable() const { return true; }
	};
}
