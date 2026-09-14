#include "Koi/Engine/Graphics/RenderPasses/RenderPass.h"
#include "Koi/Engine/Graphics/RHI/CommandList.h"
#include "Koi/Engine/Graphics/RHI/GpuProfiling.h"

namespace koi
{
	void PassList::Execute(CommandList& cmd)
	{
		ZoneScoped;
		for (auto& pass : m_passes)
		{
			KOI_GPU_ZONE_TRANSIENT(cmd, passZone, pass->GetName().data());
			pass->Execute(cmd);
		}
	}

	void PassList::ResetAll()
	{
		ZoneScoped;
		for (auto& pass : m_passes)
		{
			pass->Reset();
		}
	}

	void PassList::Clear()
	{
		m_passes.clear();
	}
}
