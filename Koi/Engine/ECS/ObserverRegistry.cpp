#include "Koi/Engine/ECS/ObserverRegistry.h"

namespace koi
{
	ObserverRegistry& ObserverRegistry::Get()
	{
		static ObserverRegistry s_instance;
		return s_instance;
	}

	std::vector<std::unique_ptr<IObserver>> ObserverRegistry::Instantiate() const
	{
		std::vector<std::unique_ptr<IObserver>> observers;
		observers.reserve(m_factories.size());

		for (const auto& factory : m_factories)
		{
			observers.push_back(factory());
		}
		return observers;
	}
}
