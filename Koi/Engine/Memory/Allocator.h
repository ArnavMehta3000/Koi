#pragma once
#include "Koi/Engine/Core/Types.h"
#include <cstddef>

namespace koi::internal
{
	[[nodiscard]] void* Allocate(std::size_t size, std::size_t align) noexcept;
	void Free(void* p) noexcept;

	[[nodiscard]] u64 GetAllocationCount() noexcept;
}
