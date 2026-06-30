module;
#include "Core/Config.h"
#include <unordered_set>
module Koi.Graphics.RenderPass;

import Koi.Core.Types;
import Koi.Graphics.RHI.CommandList;
#include "Core/Logger.h"

namespace koi
{
	void RenderGraph::Compile()
	{
		std::unordered_set<u64> written;

		for (const auto& pass : m_passes)
		{
			for (const RenderPassAttachment& input : pass->GetInputs())
			{
				if (input.Texture && !written.count(static_cast<u64>(input.Texture.GetId())))
					KOI_LOG_TRACE("RenderGraph: '{}' reads '{}' - externally produced (no prior pass writes it)",
					              pass->GetName(), input.Name);
			}
			for (const RenderPassAttachment& output : pass->GetOutputs())
			{
				if (output.Texture)
					written.insert(static_cast<u64>(output.Texture.GetId()));
			}
		}

		m_dirty = false;
	}

	void RenderGraph::Execute(CommandList& cmd)
	{
		if (m_dirty)
			Compile();
		for (auto& pass : m_passes)
			pass->Execute(cmd);
	}

	void RenderGraph::Clear()
	{
		m_passes.clear();
		m_dirty = true;
	}
}
