#pragma once
#include "Koi/Engine/ECS/Observer.h"

namespace koi
{
	// Observers are registered once as factories, then instantiated by every created World
	class ObserverRegistry
	{
	public:
		[[nodiscard]] static ObserverRegistry& Get();

		template<typename T, typename... Args>
		void Register(Args&&... args)
		{
			m_factories.push_back([=] { return std::unique_ptr<IObserver>(std::make_unique<T>(args...)); });
		}

		[[nodiscard]] std::vector<std::unique_ptr<IObserver>> Instantiate() const;

	private:
		std::vector<std::function<std::unique_ptr<IObserver>()>> m_factories;
	};
}
