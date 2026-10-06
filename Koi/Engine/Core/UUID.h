#pragma once
#include "Koi/Engine/Core/Types.h"
#include <stduuid/uuid.h>

namespace koi
{
	using UUID = uuids::uuid;

	namespace utils
	{
		// Version 4, from a process-wide generator seeded once
		[[nodiscard]] UUID NewUUID();

		[[nodiscard]] UUID UUIDFromName(std::string_view name);

		// A nil UUID stringifies to all zeroes
		[[nodiscard]] inline std::string ToString(const UUID& id) { return uuids::to_string(id); }

		// Nil on anything unparseable
		[[nodiscard]] inline UUID UUIDFromString(std::string_view str) { return UUID::from_string(str).value_or(UUID{}); }
	}
}
