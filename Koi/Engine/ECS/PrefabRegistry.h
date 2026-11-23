#pragma once
#include "Koi/Engine/Resources/PrefabAsset.h"
#include <concepts>
#include <string>
#include <utility>
#include <vector>

namespace koi
{
	// Maps a .prefab.toml's class to the C++ prefab type that holds its template
	class PrefabRegistry
	{
	public:
		using Factory = std::unique_ptr<IPrefabTemplate>(*)();

		[[nodiscard]] static PrefabRegistry& Get();

		template<std::derived_from<IPrefabTemplate> P>
		void Register()
		{
			Register(P::kKey, []() -> std::unique_ptr<IPrefabTemplate> { return std::make_unique<P>(); });
		}

		void Register(std::string_view className, Factory factory);

		[[nodiscard]] bool Contains(std::string_view className) const;
		[[nodiscard]] std::unique_ptr<IPrefabTemplate> Create(std::string_view className) const;

	private:
		std::vector<std::pair<std::string, Factory>> m_factories;
	};
}
