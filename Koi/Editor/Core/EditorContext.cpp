#include "Koi/Editor/Core/EditorContext.h"

namespace koi::editor
{
	EditorContext::~EditorContext()
	{
		// Newest first, so a service outlives everything added after it
		while (!m_services.empty())
		{
			m_services.pop_back();
		}
	}

	void EditorContext::Tick(f32 dt)
	{
		for (const Entry& entry : m_services)
		{
			if (entry.Tick) entry.Tick(entry.Service.get(), dt);
		}
	}

	void EditorContext::Shutdown()
	{
		for (auto it = m_services.rbegin(); it != m_services.rend(); ++it)
		{
			if (it->Shutdown) it->Shutdown(it->Service.get());
		}
	}
}
