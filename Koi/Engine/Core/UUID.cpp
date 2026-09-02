#include "Koi/Engine/Core/UUID.h"
#include <random>

namespace koi
{
	namespace
	{
		// Not thread-safe
		uuids::uuid_random_generator& Generator()
		{
			static std::mt19937 engine = []
			{
				std::random_device rd;
				std::seed_seq sequence{ rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd() };
				return std::mt19937(sequence);
			}();

			static uuids::uuid_random_generator generator(engine);
			return generator;
		}
	}

	UUID NewUUID()
	{
		return Generator()();
	}

	UUID UUIDFromName(std::string_view name)
	{
		// Fixed, arbitrary, and must never change - every proc:// identity derives from it
		static const UUID kKoiNamespace = UUIDFromString("6b1a9f3c-5d84-4d2e-9c17-0a5e7b62d401");
		static uuids::uuid_name_generator generator(kKoiNamespace);
		return generator(name);
	}
}
