#include "Koi/Editor/Reloader.h"
#include "Koi/Engine/Logging/Logger.h"

namespace koi::editor
{
	void Reloader::Flush()
	{
		if (!m_pending) return;

		const ReloadRequest request = *std::exchange(m_pending, std::nullopt);
		KOI_LOG(Editor, Info, "Hot-reloading {} registered target(s)", m_targets.size());
		for (IReloadable* reloadable : m_targets)
		{
			reloadable->Reload(request);
		}
	}
}
