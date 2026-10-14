#include "Koi/Engine/Graphics/RenderGraph/RenderGraph.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Graphics/RHI/CommandList.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Graphics/RHI/GpuProfiling.h"
#include <cassert>
#include <ranges>

namespace koi
{
	namespace
	{
		constexpr D3D12_RESOURCE_STATES kShaderRead = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
	}

	void RGBuilder::Read(RGTexture texture)
	{
		if (texture)
		{
			(void)m_graph.Declare(m_passIndex, texture, RenderGraph::Access::ShaderRead);
		}
	}

	RGTexture RGBuilder::Write(RGTexture texture)
	{
		if (!texture) return {};
		return m_graph.Declare(m_passIndex, texture, RenderGraph::Access::ShaderWrite);
	}

	RGTexture RGBuilder::ColorTarget(RGTexture texture, LoadOp load, const Color& clear)
	{
		if (!texture) return {};

		const RGTexture written = m_graph.Declare(m_passIndex, texture, RenderGraph::Access::ColorTarget);
		m_graph.m_passes[m_passIndex].ColorTargets.push_back({ .Resource = texture.Index, .Load = load, .Clear = clear });
		assert(m_graph.m_passes[m_passIndex].ColorTargets.size() <= MAX_RENDER_TARGETS && "Too many color targets in one pass");
		return written;
	}

	RGTexture RGBuilder::DepthTarget(RGTexture texture, LoadOp load, f32 clearDepth)
	{
		if (!texture) return {};

		auto& pass = m_graph.m_passes[m_passIndex];
		assert(!pass.DepthTarget && "A pass has at most one depth target");

		const RGTexture written = m_graph.Declare(m_passIndex, texture, RenderGraph::Access::DepthTarget);
		pass.DepthTarget = RenderGraph::Attachment{ .Resource = texture.Index, .Load = load, .ClearDepth = clearDepth };
		return written;
	}

	TextureHandle PassContext::Texture(RGTexture texture) const
	{
		return m_graph.Resolve(texture);
	}

	DescriptorIndex PassContext::SRV(RGTexture texture) const
	{
		const koi::Texture* tex = m_graph.GetDevice().Get(m_graph.Resolve(texture));
		return tex ? tex->SRVIndex : INVALID_DESCRIPTOR;
	}

	DescriptorIndex PassContext::UAV(RGTexture texture) const
	{
		const koi::Texture* tex = m_graph.GetDevice().Get(m_graph.Resolve(texture));
		return tex ? tex->UAVIndex : INVALID_DESCRIPTOR;
	}

	bool PassContext::Bind(const std::expected<BoundPipeline, PipelineError>& pipeline)
	{
		if (!pipeline) return false;

		m_bound = *pipeline;
		m_cmd.SetPipeline(m_bound.Handle);
		return true;
	}

	bool RasterContext::SetPipeline(const RasterPipeline& pipeline)
	{
		return Bind(m_graph.GetPipelines().Get(pipeline, m_targets));
	}

	bool ComputeContext::SetPipeline(const ComputePipeline& pipeline)
	{
		return Bind(m_graph.GetPipelines().Get(pipeline));
	}

	void RenderGraph::Init(GfxDevice& device, PipelineCache& pipelines)
	{
		m_device    = &device;
		m_pipelines = &pipelines;
		m_pool.Init(device);
	}

	void RenderGraph::Shutdown()
	{
		Reset();
		m_inspection = {};
		m_pool.Shutdown();
	}

	void RenderGraph::Reset()
	{
		m_resources.clear();
		m_passes.clear();
	}

	RGTexture RenderGraph::CreateTexture(const TextureDesc& desc, std::string_view name)
	{
		Resource& r = m_resources.emplace_back(Resource{ .Name = std::string(name), .Desc = desc });

		// Every intermediate stays viewable in the editor
		r.Desc.Usage |= TextureUsage::ShaderRead;
		r.Desc.DebugName = {};
		return { .Index = static_cast<u16>(m_resources.size() - 1) };
	}

	RGTexture RenderGraph::ImportTexture(TextureHandle texture, std::string_view name)
	{
		const koi::Texture* tex = m_device->Get(texture);
		if (!tex) return {};

		Resource& r = m_resources.emplace_back(Resource{ .Name = std::string(name), .Desc = tex->Desc, .Texture = texture, .Imported = true });
		r.Desc.DebugName = {};
		return { .Index = static_cast<u16>(m_resources.size() - 1) };
	}

	const TextureDesc& RenderGraph::GetDesc(RGTexture texture) const
	{
		assert(texture.Index < m_resources.size() && "Invalid graph texture");
		return m_resources[texture.Index].Desc;
	}

	TextureHandle RenderGraph::Resolve(RGTexture texture) const
	{
		return texture && texture.Index < m_resources.size() ? m_resources[texture.Index].Texture : TextureHandle();
	}

	u32 RenderGraph::BeginPass(std::string_view name)
	{
		const char* interned = m_names.emplace(name).first->c_str();
		m_passes.push_back(Pass{ .Name = interned });
		return static_cast<u32>(m_passes.size() - 1);
	}

	RGTexture RenderGraph::Declare(u32 passIndex, RGTexture texture, Access access)
	{
		assert(texture.Index < m_resources.size() && "Invalid graph texture");
		Resource& r = m_resources[texture.Index];
		assert(texture.Version == r.Version && "Pass uses a stale version of a graph texture");

		m_passes[passIndex].Uses.push_back({ texture, access });

		// Shader read or depth/render target
		const TextureUsage needed = access == Access::ShaderRead  ? TextureUsage::ShaderRead
		                          : access == Access::ShaderWrite ? TextureUsage::ShaderWrite
		                          : access == Access::ColorTarget ? TextureUsage::RenderTarget
		                          :                                 TextureUsage::DepthStencil;
		if (r.Imported)
		{
			assert(r.Desc.Usage.Has(needed) && "Imported texture was not created for this use");
		}
		else
		{
			r.Desc.Usage |= needed;
		}

		if (access == Access::ShaderRead) return texture;

		++r.Version;
		return { .Index = texture.Index, .Version = r.Version };
	}

	void RenderGraph::BindTargets(CommandList& cmd, const Pass& pass)
	{
		if (pass.ColorTargets.empty() && !pass.DepthTarget) return;

		std::array<TextureHandle, MAX_RENDER_TARGETS> colors{};
		for (const auto& [i, att] : std::views::enumerate(pass.ColorTargets))
		{
			colors[i] = m_resources[att.Resource].Texture;
		}

		const TextureHandle depth = pass.DepthTarget ? m_resources[pass.DepthTarget->Resource].Texture : TextureHandle();
		cmd.SetRenderTarget(std::span(colors.data(), pass.ColorTargets.size()), depth);

		const u16 sizeFrom = pass.ColorTargets.empty() ? pass.DepthTarget->Resource : pass.ColorTargets.front().Resource;
		const TextureDesc& size = m_resources[sizeFrom].Desc;
		cmd.SetViewportAndScissor(size.Width, size.Height);

		for (const auto& [i, att] : std::views::enumerate(pass.ColorTargets))
		{
			if (att.Load == LoadOp::Clear)
			{
				const std::array<f32, 4> clear = { att.Clear.x, att.Clear.y, att.Clear.z, att.Clear.w };
				cmd.ClearRenderTarget(colors[i], clear.data());
			}
		}

		if (pass.DepthTarget && pass.DepthTarget->Load == LoadOp::Clear)
		{
			cmd.ClearDepthStencil(depth, pass.DepthTarget->ClearDepth);
		}
	}

	TargetFormats RenderGraph::FormatsOf(const Pass& pass) const
	{
		TargetFormats formats{ .ColorCount = static_cast<u32>(pass.ColorTargets.size()) };
		for (const auto& [i, att] : std::views::enumerate(pass.ColorTargets))
		{
			formats.Colors[i] = m_resources[att.Resource].Desc.Format;
		}

		if (pass.DepthTarget)
		{
			formats.Depth = m_resources[pass.DepthTarget->Resource].Desc.Format;
		}
		return formats;
	}

	void RenderGraph::Execute(CommandList& cmd)
	{
		KOI_SCOPE("Record Passes");

		for (Resource& r : m_resources)
		{
			if (r.Imported) continue;

			TextureDesc desc = r.Desc;
			desc.DebugName   = r.Name;
			r.Texture        = m_pool.Acquire(desc);
		}

		for (Pass& pass : m_passes)
		{
			KOI_GPU_ZONE_TRANSIENT(cmd, passZone, pass.Name);

			for (const Use& use : pass.Uses)
			{
				const TextureHandle texture = m_resources[use.Texture.Index].Texture;
				const D3D12_RESOURCE_STATES state = use.Access == Access::ShaderRead  ? kShaderRead
				                                  : use.Access == Access::ShaderWrite ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS
				                                  : use.Access == Access::ColorTarget ? D3D12_RESOURCE_STATE_RENDER_TARGET
				                                  :                                     D3D12_RESOURCE_STATE_DEPTH_WRITE;
				cmd.TransitionBarrier(texture, state);
			}
			cmd.FlushBarriers();

			assert((!pass.Compute || (pass.ColorTargets.empty() && !pass.DepthTarget)) && "Compute passes write through Write(), not targets");
			BindTargets(cmd, pass);

			if (pass.Execute)
			{
				pass.Execute(cmd, FormatsOf(pass));
			}
		}

		Inspect();

		// Released, not destroyed (marked not in use in pool).Since the editor may still sample them this frame
		for (const Resource& r : m_resources)
		{
			if (!r.Imported && r.Texture)
			{
				m_pool.Release(r.Texture);
			}
		}
		m_pool.EndFrame();
		Reset();
	}

	void RenderGraph::NotifyWrites(u32 passIndex)
	{
		// Passes the observer adds are not reported back to it
		if (!m_observer || m_notifying) return;
		m_notifying = true;

		// Copied: the observer may add passes, which can reallocate m_passes
		const std::vector<Use> uses = m_passes[passIndex].Uses;
		for (const Use& use : uses)
		{
			if (use.Access == Access::ShaderRead) continue;

			const RGTexture written
			{
				.Index = use.Texture.Index,
				.Version = static_cast<u16>(use.Texture.Version + 1)
			};
			const std::string name = m_resources[use.Texture.Index].Name;  // the observer may grow m_resources

			m_observer->OnWrite(*this, name, written);
		}
		m_notifying = false;
	}

	void RenderGraph::Inspect()
	{
		m_inspection.Passes.clear();
		for (const Pass& pass : m_passes)
		{
			RGInspection::Pass& info = m_inspection.Passes.emplace_back(RGInspection::Pass{ .Name = pass.Name });
			for (const Use& use : pass.Uses)
			{
				if (use.Access == Access::ShaderRead) continue;

				const Resource& r = m_resources[use.Texture.Index];
				info.Writes.push_back({ .Name = r.Name, .Version = static_cast<u16>(use.Texture.Version + 1), .Texture = r.Texture });
			}
		}
	}
}
