// Lightweight object wrapper
#include "Core/Types.h"

namespace koi
{
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
		operator T*() const { return static_cast<T*>(AsPointer); }
	};
}
