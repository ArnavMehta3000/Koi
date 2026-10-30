#pragma once
#include "Koi/Editor/Undo/ComponentOf.h"
#include "Koi/Editor/Undo/UndoStack.h"
#include "Koi/Engine/ECS/World.h"
#include <string_view>

namespace koi::editor
{
	// Draws one component's editor UI
	class IComponentInspector
	{
	public:
		virtual ~IComponentInspector() = default;

		[[nodiscard]] virtual std::string_view Key() const = 0;
		virtual void Draw(Entity e) = 0;

		// Watches this component on e until the scope dies, so the panel can undo edits without knowing the type
		[[nodiscard]] virtual UndoScope Watch(UndoStack& undo, std::string_view label, Entity e) const = 0;
	};

	template <typename T>
	class ComponentInspector : public IComponentInspector
	{
	public:
		using Component = T;

		[[nodiscard]] std::string_view Key() const final { return T::kKey; }

		[[nodiscard]] UndoScope Watch(UndoStack& undo, std::string_view label, Entity e) const final
		{
			return undo.Scope(label, ComponentOf<T>(e));
		}
	};
}
