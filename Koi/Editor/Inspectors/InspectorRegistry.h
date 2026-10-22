#pragma once
#include "Koi/Editor/Inspectors/ComponentInspector.h"
#include "Koi/Engine/Core/Types.h"
#include <memory>
#include <utility>
#include <vector>

namespace koi::editor
{
	class InspectorRegistry : public NonCopyable
	{
	public:
		void Add(std::unique_ptr<IComponentInspector> inspector);

		template<typename T, typename... Args>
		T& Emplace(Args&&... args)
		{
			auto inspector = std::make_unique<T>(std::forward<Args>(args)...);
			T& ref = *inspector;
			Add(std::move(inspector));
			return ref;
		}

		[[nodiscard]] IComponentInspector* Find(std::string_view key);

		// Transform, mesh, material, camera and light
		void RegisterBuiltins();

	private:
		std::vector<std::unique_ptr<IComponentInspector>> m_inspectors;
	};
}
