#include "Graphics/RenderPasses/RenderPass.h"
#include "Graphics/RHI/CommandList.h"
#include "Core/Logger.h"
#include <unordered_set>

namespace koi
{
	void RenderGraph::Compile()
	{
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
		}

		m_dirty = false;
	}

	void RenderGraph::Execute(CommandList& cmd)
	{
		if (m_dirty) Compile();

		for (auto& pass : m_passes)
		{
			pass->Execute(cmd);
		}
	}

	void RenderGraph::ResetAll()
	{
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
