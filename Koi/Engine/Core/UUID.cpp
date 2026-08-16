#include "Koi/Engine/Core/UUID.h"
#include <random>

namespace koi
{
	namespace
	{
		// Not thread-safe; minting is a load/edit-time operation, never a frame path
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
}
