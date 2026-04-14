#pragma once
#include "Core/Config.h"
#include "Kirin/Common/Assert.h"

#define KOI_DEBUGBREAK() KRN_DEBUGBREAK()

#if KOI_DEBUG
	#define KOI_ASSERT(x) KRN_ASSERT(x)
	#define KOI_ASSERT_MSG(x, msg) KRN_ASSERT_MSG(x, msg)
#else
	#define KOI_ASSERT(x)
	#define KOI_ASSERT_MSG(x, msg)
#endif

#define KOI_VERIFY(x) KRN_VERIFY(x)
#define KOI_VERIFY_MSG(x, msg) KRN_VERIFY_MSG(x, msg)

namespace koi
{
	// Pull krn standard types into koi namespace
	using namespace krn::types;
}
