// Lightweight object wrapper
#pragma once
#include "Koi/Engine/Core/Types.h"

namespace koi
{
	// Type erased object
	struct Object
	{
		union
		{
			u64 AsInteger;
			void* AsPointer;
		};

		Object(u64 value) : AsInteger(value) {}
		Object(void* value) : AsPointer(value) {}

		template <typename T>
		T* As() const { return static_cast<T*>(AsPointer); }
	};
}
