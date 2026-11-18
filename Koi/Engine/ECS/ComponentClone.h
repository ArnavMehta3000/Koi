#pragma once
#include "Koi/Engine/ECS/Components.h"

namespace koi
{
	// Copies a component for another owner
	template<typename T>
	struct ComponentClone
	{
		[[nodiscard]] static T Copy(const T& component) { return component; }
		static void Drop(T&) {}
	};

	template<>
	struct ComponentClone<MeshRef>
	{
		[[nodiscard]] static MeshRef Copy(const MeshRef& mesh);
		static void Drop(MeshRef& mesh);
	};

	template<>
	struct ComponentClone<MaterialRef>
	{
		[[nodiscard]] static MaterialRef Copy(const MaterialRef& material);
		static void Drop(MaterialRef& material);
	};
}
