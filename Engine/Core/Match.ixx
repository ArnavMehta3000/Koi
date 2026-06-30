module;
export module Koi.Core.Match;

export namespace koi
{
	template<class... Ts> struct Match : Ts... { using Ts::operator()...; };
	template<class... Ts> Match(Ts...) -> Match<Ts...>;
}
