#pragma once
#include "Koi/Engine/Core/Types.h"
#include <string_view>

namespace koi::internal
{
	// The full template argument from the MSVC signature, e.g. "struct koi::Transform"
	template <typename T>
	constexpr std::string_view QualifiedTypeName()
	{
		std::string_view name = __FUNCSIG__;
		name = name.substr(name.find("QualifiedTypeName<") + 18);
		return name.substr(0, name.rfind(">("));
	}

	[[nodiscard]] u32 TypeIndexOf(std::string_view domain, std::string_view qualifiedName);

	template <typename Domain, typename T>
	u32 TypeIndex()
	{
		static const u32 index = TypeIndexOf(QualifiedTypeName<Domain>(), QualifiedTypeName<T>());
		return index;
	}
}
