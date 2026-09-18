#pragma once
#include "Koi/Engine/ECS/System.h"

namespace koi
{
	// Systems are registered once as factories, then instantiated by every World that is created
	class SystemRegistry
	{
	public:
		[[nodiscard]] static SystemRegistry& Get();

		template<typename T, typename... Args>
		void Register(Args&&... args)
		{
			m_factories.push_back([=] { return std::unique_ptr<ISystem>(std::make_unique<T>(args...)); });
		}

		[[nodiscard]] std::vector<std::unique_ptr<ISystem>> Instantiate() const;

	private:
		std::vector<std::function<std::unique_ptr<ISystem>()>> m_factories;
	};
}
