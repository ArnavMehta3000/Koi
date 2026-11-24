#pragma once
#include "Koi/Engine/Logging/Logger.h"
#include <concepts>
#include <string_view>

namespace koi
{
	// A module is a struct with kName + a static Register(); an editor half, if any, is a nested `Editor` type
	template<typename M>
	concept Module = requires
	{
		{ M::kName } -> std::convertible_to<std::string_view>;
		M::Register();
	};

	template<Module... Ms>
	struct ModuleList
	{
		static void Register() { (RegisterOne<Ms>(), ...); }

	private:
		template<Module M>
		static void RegisterOne()
		{
			KOI_LOG(Engine, Debug, "Registering module '{}'", std::string_view(M::kName));
			M::Register();
		}
	};
}
