#pragma once
#include "Koi/Engine/Logging/Logger.h"
#include <string_view>

KOI_LOG_CATEGORY_IN(Physics, Engine)

namespace koi
{
	struct PhysicsModule
	{
		static constexpr std::string_view kName = "Physics";

		struct Editor;

		static void Register();
	};
}
