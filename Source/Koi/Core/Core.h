#pragma once
#include "Core/Config.h"
#include "Core/ClassUtils.h"
#include "Kirin/Common/Assert.h"

#define KOI_DEBUGBREAK() KRN_DEBUGBREAK()

#define KOI_ASSERT(cond)            KRN_ASSERT(cond)
#define KOI_ASSERT_MSG(cond, msg)   KRN_ASSERT_MSG(cond, msg)
#define KOI_VERIFY(cond)            KRN_VERIFY(cond)
#define KOI_VERIFY_MSG(cond, msg)   KRN_VERIFY_MSG(cond, msg)
#define KOI_UNREACHABLE(msg)        KRN_UNREACHABLE(msg)

namespace koi
{
	// Pull krn standard types into koi namespace
	using namespace krn::types;
}
