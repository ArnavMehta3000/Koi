// Contains common RHI types
#pragma once
#include "Core/Core.h"

namespace koi::gfx
{
	static constexpr u64 FRAME_IN_FLIGHT = 3;

	enum class API
	{
		Headless,
		DX12
	};

	inline std::string_view ToString(API api)
	{
		switch (api)
		{
		case API::Headless: return "Headless";
		case API::DX12:     return "DX12";
		default:            return "<Unknown>";
		}
	}

}
