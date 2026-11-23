#include "Koi/Engine/Core/TypeIndex.h"
#include <mutex>
#include <string>
#include <unordered_map>

namespace koi::internal
{
	u32 TypeIndexOf(std::string_view domain, std::string_view qualifiedName)
	{
		static std::mutex mutex;
		static std::unordered_map<std::string, std::unordered_map<std::string, u32>> domains;

		std::lock_guard lock(mutex);
		auto& ids = domains[std::string(domain)];
		return ids.try_emplace(std::string(qualifiedName), static_cast<u32>(ids.size())).first->second;
	}
}
