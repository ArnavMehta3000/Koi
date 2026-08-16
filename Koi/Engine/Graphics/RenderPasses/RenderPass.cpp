#include "Koi/Engine/Graphics/RenderPasses/RenderPass.h"
#include "Koi/Engine/Graphics/RHI/CommandList.h"
#include "Koi/Engine/Graphics/RHI/GpuProfiling.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Memory/MemoryTracker.h"
#include <unordered_set>

namespace koi
{
	void RenderGraph::Compile()
	{
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);
		KOI_LOG_DEBUG("RenderGraph: compiling {} pass(es)", m_passes.size());

		// Check connectivity using names of attachments
		std::unordered_set<std::string_view> written;

		for (const auto& pass : m_passes)
		{
			for (const RenderPassAttachment& input : pass->GetInputs())
			{
				if (!written.count(input.Name))
				{
					KOI_LOG_TRACE("RenderGraph: '{}' reads '{}' - externally produced (no prior pass writes it)",
						pass->GetName(), input.Name);
				}
			}

			for (const RenderPassAttachment& output : pass->GetOutputs())
			{
				written.insert(output.Name);
			}

			KOI_LOG_TRACE("RenderGraph: pass '{}' ({} input(s), {} output(s))",
				pass->GetName(), pass->GetInputs().size(), pass->GetOutputs().size());
		}

		m_dirty = false;
	}

	void RenderGraph::Execute(CommandList& cmd)
	{
		ZoneScoped;
		if (m_dirty) Compile();

		for (auto& pass : m_passes)
		{
			KOI_GPU_ZONE_TRANSIENT(cmd, passZone, pass->GetName().data());
			pass->Execute(cmd);
		}
	}

	void RenderGraph::ResetAll()
	{
		ZoneScoped;
		for (auto& pass : m_passes)
		{
			pass->Reset();
		}
	}

	void RenderGraph::Clear()
	{
		m_passes.clear();
		m_dirty = true;
	}
}
