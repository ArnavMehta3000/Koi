#pragma once
#include "Koi/Engine/Core/Config.h"
#include <format>
#include <source_location>
#include <string_view>

namespace koi::internal
{
	void OnAssertFailed(std::string_view condition, std::string_view message, const std::source_location& where);
}

#if KOI_DEBUG
	#define KOI_ASSERT(cond, ...)                                                                                          \
		do                                                                                                                 \
		{                                                                                                                  \
			if (!(cond)) [[unlikely]]                                                                                      \
			{                                                                                                              \
				::koi::internal::OnAssertFailed(#cond, ::std::format("" __VA_ARGS__), ::std::source_location::current());  \
			}                                                                                                              \
		} while (0)
#else
	#define KOI_ASSERT(cond, ...) do { (void)sizeof(cond); } while (0)
#endif
