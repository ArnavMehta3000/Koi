module;
export module Koi.Core.Object;

import Koi.Core.Types;

export namespace koi
{
	struct Object
	{
		union
		{
			u64   AsInteger;
			void* AsPointer;
		};

		Object(u64 value)   : AsInteger(value) {}
		Object(void* value) : AsPointer(value) {}

		template <typename T>
		T* As() const { return static_cast<T*>(AsPointer); }
	};
}
