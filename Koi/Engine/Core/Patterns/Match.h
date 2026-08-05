#pragma once

namespace koi
{
	// To be used along with std::variant and std::visit
	template<class... Ts> struct Match : Ts... { using Ts::operator()...; };
	template<class... Ts> Match(Ts...) -> Match<Ts...>;
}
