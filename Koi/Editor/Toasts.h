#pragma once
#include "Koi/Engine/Core/Types.h"
#include <string>
#include <vector>

namespace koi::editor
{
	enum class ToastKind : u8
	{
		Info,
		Success,
		Warning,
		Error,
	};

	// Short-lived notices stacked in the bottom-right corner, newest at the bottom
	class Toasts
	{
	public:
		void Push(ToastKind kind, std::string text);
		void Draw();

	private:
		struct Toast
		{
			ToastKind   Kind;
			std::string Text;
			f64         Born = 0.0;
			u32         Id   = 0;
		};

	private:
		std::vector<Toast> m_toasts;
		u32                m_nextId = 0;
	};
}
