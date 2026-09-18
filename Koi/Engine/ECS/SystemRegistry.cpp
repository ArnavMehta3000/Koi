#include "Koi/Engine/ECS/SystemRegistry.h"

namespace koi
{
	SystemRegistry& SystemRegistry::Get()
	{
		static SystemRegistry s_instance;
		return s_instance;
	}

	std::vector<std::unique_ptr<ISystem>> SystemRegistry::Instantiate() const
	{
		std::vector<std::unique_ptr<ISystem>> systems;
		systems.reserve(m_factories.size());

		for (const auto& factory : m_factories)
		{
			systems.push_back(factory());
		}
		return systems;
	}
}
