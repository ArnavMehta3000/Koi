#pragma once
#include "Kirin/Common/StandardTypes.h"

namespace koi
{
	// Pull krn standard types into koi namespace
	using namespace krn::types;

    struct NonCopyable
    {
        NonCopyable() = default;
        ~NonCopyable() = default;

        NonCopyable(const NonCopyable&) = delete;
        NonCopyable& operator=(const NonCopyable&) = delete;
    };

    struct NonMovable
    {
        NonMovable() = default;
        ~NonMovable() = default;

        NonMovable(NonMovable&&) = delete;
        NonMovable& operator=(NonMovable&&) = delete;
    };

    struct NonCopyableNonMovable : NonCopyable, NonMovable
    {
        NonCopyableNonMovable() = default;
    };
}