#pragma once
#include "Koi/Engine/Runtime/Module.h"

namespace koi::editor
{
	class EditorContext;

	// Registers the editor half of every module in a game's ModuleList. Include each module's Editor/ header first
	template<typename List>
	struct EditorModules;

	template<Module... Ms>
	struct EditorModules<ModuleList<Ms...>>
	{
		static void Register(EditorContext& ctx) { (RegisterOne<Ms>(ctx), ...); }

	private:
		template<Module M>
		static void RegisterOne(EditorContext& ctx)
		{
			if constexpr (requires { typename M::Editor; })
			{
				M::Editor::Register(ctx);
			}
		}
	};
}
