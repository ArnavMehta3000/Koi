#pragma once
#include "Koi/Engine/ECS/ComponentType.h"
#include <memory>
#include <span>
#include <vector>

namespace koi
{
	class ComponentRegistry
	{
	public:
		[[nodiscard]] static ComponentRegistry& Get();

		void Register(std::unique_ptr<IComponentType> type);

		template <typename T, typename... Args>
		T& Register(Args&&... args)
		{
			auto type = std::make_unique<T>(std::forward<Args>(args)...);
			T& ref = *type;
			Register(std::unique_ptr<IComponentType>(std::move(type)));
			return ref;
		}

		[[nodiscard]] inline std::span<const std::unique_ptr<IComponentType>> All() const { return m_types; }
		[[nodiscard]] const IComponentType* Find(std::string_view key) const;

		// Called once by the Engine, before anything can load a scene
		void RegisterEngineComponents();

	private:
		std::vector<std::unique_ptr<IComponentType>> m_types;
	};
}
