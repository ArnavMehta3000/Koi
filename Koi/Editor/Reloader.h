#pragma once
#include "Koi/Engine/Runtime/Reloadable.h"
#include <optional>
#include <utility>
#include <vector>

namespace koi::editor
{
	// F5 / Shift+F5: requested during the ImGui frame, run before any command list records
	class Reloader
	{
	public:
		void Register(IReloadable* reloadable) { m_targets.push_back(reloadable); }
		void Request(ReloadRequest request) { m_pending = request; }

		void Flush();

	private:
		std::vector<IReloadable*>    m_targets;
		std::optional<ReloadRequest> m_pending;
	};
}
