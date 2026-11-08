#include "Koi/Engine/Graphics/RenderGraph/RenderGraph.h"
#include "Koi/Engine/Core/Assert.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Graphics/RHI/CommandList.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Graphics/RHI/GpuProfiling.h"
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

	void RGBuilder::Read(RGBuffer buffer)
	{
		if (buffer)
		{
			std::ignore = m_graph.Declare(m_passIndex, buffer, RenderGraph::Access::ShaderRead);
		}
	}

	RGBuffer RGBuilder::Write(RGBuffer buffer)
	{
		if (!buffer) return {};
		return m_graph.Declare(m_passIndex, buffer, RenderGraph::Access::ShaderWrite);
	}

	void RGBuilder::CopyFrom(RGBuffer buffer)
	{
		if (buffer)
		{
			(void)m_graph.Declare(m_passIndex, buffer, RenderGraph::Access::CopySource);
		}
	}

	RGTexture RGBuilder::ColorTarget(RGTexture texture, LoadOp load, const Color& clear)
	{
		if (!texture) return {};

		auto& pass = m_graph.m_passes[m_passIndex];
		KOI_ASSERT(pass.ColorCount < MAX_RENDER_TARGETS, "Too many color targets in one pass");

		const RGTexture written = m_graph.Declare(m_passIndex, texture, RenderGraph::Access::ColorTarget);
		pass.ColorTargets[pass.ColorCount++] = { .Resource = texture.Index, .Load = load, .Clear = clear };
		return written;
	}

	RGTexture RGBuilder::DepthTarget(RGTexture texture, LoadOp load, f32 clearDepth)
	{
		if (!texture) return {};

		auto& pass = m_graph.m_passes[m_passIndex];
		KOI_ASSERT(!pass.DepthTarget, "A pass has at most one depth target");

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

	BufferHandle PassContext::Buffer(RGBuffer buffer) const
	{
		return m_graph.Resolve(buffer);
	}

	DescriptorIndex PassContext::SRV(RGBuffer buffer) const
	{
		const koi::Buffer* buf = m_graph.GetDevice().Get(m_graph.Resolve(buffer));
		return buf ? buf->SRVIndex : INVALID_DESCRIPTOR;
	}

	DescriptorIndex PassContext::UAV(RGBuffer buffer) const
	{
		const koi::Buffer* buf = m_graph.GetDevice().Get(m_graph.Resolve(buffer));
		return buf ? buf->UAVIndex : INVALID_DESCRIPTOR;
	}

	void PassContext::CopyBuffer(BufferHandle dst, u64 dstOffset, RGBuffer src, u64 size)
	{
		m_cmd.CopyBuffer(dst, dstOffset, m_graph.Resolve(src), 0, size);
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
		// No thread group means no reflection data, and the dispatch maths would divide by zero
		return Bind(m_graph.GetPipelines().Get(pipeline)) && m_bound.Info.ThreadGroup[0] != 0;
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
		m_buffers.clear();
		m_passes.clear();
		m_uses.clear();
		m_blackboard.Clear();
	}

	const std::string& RenderGraph::Intern(std::string_view name)
	{
		auto it = m_names.find(name);
		if (it == m_names.end())
		{
			it = m_names.emplace(name).first;
		}
		return *it;
	}

	RGTexture RenderGraph::CreateTexture(const TextureDesc& desc, std::string_view name)
	{
		Resource& r = m_resources.emplace_back(Resource{ .Name = Intern(name), .Desc = desc });

		// Every intermediate stays viewable in the editor
		r.Desc.Usage |= TextureUsage::ShaderRead;
		r.Desc.DebugName = {};
		return { .Index = static_cast<u16>(m_resources.size() - 1) };
	}

	RGTexture RenderGraph::ImportTexture(TextureHandle texture, std::string_view name)
	{
		const koi::Texture* tex = m_device->Get(texture);
		if (!tex) return {};

		Resource& r = m_resources.emplace_back(Resource{ .Name = Intern(name), .Desc = tex->Desc, .Texture = texture, .Imported = true });
		r.Desc.DebugName = {};
		return { .Index = static_cast<u16>(m_resources.size() - 1) };
	}

	const TextureDesc& RenderGraph::GetDesc(RGTexture texture) const
	{
		KOI_ASSERT(texture.Index < m_resources.size(), "Invalid graph texture");
		return m_resources[texture.Index].Desc;
	}

	RGBuffer RenderGraph::CreateBuffer(const BufferDesc& desc, std::string_view name)
	{
		KOI_ASSERT(desc.Stride > 0 && desc.Size >= desc.Stride, "Graph buffers are structured: give them a stride");

		BufferResource& r = m_buffers.emplace_back(BufferResource{ .Name = Intern(name), .Desc = desc });
		r.Desc.Usage    |= BufferUsage::Storage;
		r.Desc.Memory    = MemoryUsage::GpuOnly;
		r.Desc.DebugName = {};
		return { .Index = static_cast<u16>(m_buffers.size() - 1) };
	}

	RGBuffer RenderGraph::ImportBuffer(BufferHandle buffer, std::string_view name)
	{
		const koi::Buffer* buf = m_device->Get(buffer);
		if (!buf) return {};

		BufferResource& r = m_buffers.emplace_back(BufferResource{ .Name = Intern(name), .Desc = buf->Desc, .Buffer = buffer, .Imported = true });
		r.Desc.DebugName = {};
		return { .Index = static_cast<u16>(m_buffers.size() - 1) };
	}

	const BufferDesc& RenderGraph::GetDesc(RGBuffer buffer) const
	{
		KOI_ASSERT(buffer.Index < m_buffers.size(), "Invalid graph buffer");
		return m_buffers[buffer.Index].Desc;
	}

	TextureHandle RenderGraph::Resolve(RGTexture texture) const
	{
		return texture && texture.Index < m_resources.size() ? m_resources[texture.Index].Texture : TextureHandle();
	}

	BufferHandle RenderGraph::Resolve(RGBuffer buffer) const
	{
		return buffer && buffer.Index < m_buffers.size() ? m_buffers[buffer.Index].Buffer : BufferHandle();
	}

	u32 RenderGraph::BeginPass(std::string_view name)
	{
		m_passes.push_back(Pass{ .Name = Intern(name).c_str(), .FirstUse = static_cast<u32>(m_uses.size()) });
		return static_cast<u32>(m_passes.size() - 1);
	}

	RGTexture RenderGraph::Declare(u32 passIndex, RGTexture texture, Access access)
	{
		KOI_ASSERT(texture.Index < m_resources.size(), "Invalid graph texture");
		Resource& r = m_resources[texture.Index];
		KOI_ASSERT(texture.Version == r.Version, "Pass uses a stale version of a graph texture");
		PushUse(passIndex, { .Index = texture.Index, .Version = texture.Version, .Access = access });

		// Shader read or depth/render target
		const TextureUsage needed = access == Access::ShaderRead  ? TextureUsage::ShaderRead
		                          : access == Access::ShaderWrite ? TextureUsage::ShaderWrite
		                          : access == Access::ColorTarget ? TextureUsage::RenderTarget
		                          :                                 TextureUsage::DepthStencil;
		if (r.Imported)
		{
			KOI_ASSERT(r.Desc.Usage.Has(needed), "Imported texture was not created for this use");
		}
		else
		{
			r.Desc.Usage |= needed;
		}

		if (access == Access::ShaderRead) return texture;

		++r.Version;
		return { .Index = texture.Index, .Version = r.Version };
	}

	RGBuffer RenderGraph::Declare(u32 passIndex, RGBuffer buffer, Access access)
	{
		KOI_ASSERT(buffer.Index < m_buffers.size(), "Invalid graph buffer");
		BufferResource& r = m_buffers[buffer.Index];
		KOI_ASSERT(buffer.Version == r.Version, "Pass uses a stale version of a graph buffer");
		KOI_ASSERT(access == Access::ShaderRead || access == Access::ShaderWrite || access == Access::CopySource, "Buffers are read, written or copied from");
		PushUse(passIndex, { .Index = buffer.Index, .Version = buffer.Version, .Access = access, .IsBuffer = true });

		if (r.Imported)
		{
			const bool uavReady = r.Desc.Usage.Has(BufferUsage::Storage) && r.Desc.Memory == MemoryUsage::GpuOnly;
			KOI_ASSERT(access != Access::ShaderWrite || uavReady, "Imported buffer was not created for shader writes");
		}

		if (access != Access::ShaderWrite) return buffer;

		++r.Version;
		return { .Index = buffer.Index, .Version = r.Version };
	}

	void RenderGraph::PushUse(u32 passIndex, const Use& use)
	{
		Pass& pass = m_passes[passIndex];
		KOI_ASSERT(pass.FirstUse + pass.UseCount == m_uses.size(), "Uses must be declared during the pass's own setup");
		m_uses.push_back(use);
		++pass.UseCount;
	}

	void RenderGraph::BindTargets(CommandList& cmd, const Pass& pass)
	{
		if (pass.ColorCount == 0 && !pass.DepthTarget) return;

		std::array<TextureHandle, MAX_RENDER_TARGETS> colors{};
		for (const auto& [i, att] : std::views::enumerate(pass.Colors()))
		{
			colors[i] = m_resources[att.Resource].Texture;
		}

		const TextureHandle depth = pass.DepthTarget ? m_resources[pass.DepthTarget->Resource].Texture : TextureHandle();
		cmd.SetRenderTarget(std::span(colors.data(), pass.ColorCount), depth);

		const u16 sizeFrom = pass.ColorCount == 0 ? pass.DepthTarget->Resource : pass.ColorTargets.front().Resource;
		const TextureDesc& size = m_resources[sizeFrom].Desc;
		cmd.SetViewportAndScissor(size.Width, size.Height);

		for (const auto& [i, att] : std::views::enumerate(pass.Colors()))
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
		TargetFormats formats{ .ColorCount = pass.ColorCount };
		for (const auto& [i, att] : std::views::enumerate(pass.Colors()))
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

		for (BufferResource& r : m_buffers)
		{
			if (r.Imported) continue;

			BufferDesc desc = r.Desc;
			desc.DebugName  = r.Name;
			r.Buffer        = m_pool.Acquire(desc);
		}

		// Back-to-back UAV writes need a UAV barrier, a transition would be a no-op
		const auto barrier = [&](auto handle, auto* resource, D3D12_RESOURCE_STATES state)
		{
			if (resource && state == D3D12_RESOURCE_STATE_UNORDERED_ACCESS && resource->CurrentState == state)
			{
				cmd.UAVBarrier(handle);
			}
			cmd.TransitionBarrier(handle, state);
		};

		for (Pass& pass : m_passes)
		{
			KOI_GPU_ZONE_TRANSIENT(cmd, passZone, pass.Name);

			for (const Use& use : UsesOf(pass))
			{
				const D3D12_RESOURCE_STATES state = use.Access == Access::ShaderRead  ? kShaderRead
				                                  : use.Access == Access::ShaderWrite ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS
				                                  : use.Access == Access::ColorTarget ? D3D12_RESOURCE_STATE_RENDER_TARGET
				                                  : use.Access == Access::CopySource  ? D3D12_RESOURCE_STATE_COPY_SOURCE
				                                  :                                     D3D12_RESOURCE_STATE_DEPTH_WRITE;
				if (use.IsBuffer)
				{
					const BufferHandle buffer = m_buffers[use.Index].Buffer;
					barrier(buffer, m_device->Get(buffer), state);
				}
				else
				{
					const TextureHandle texture = m_resources[use.Index].Texture;
					barrier(texture, m_device->Get(texture), state);
				}
			}
			cmd.FlushBarriers();

			KOI_ASSERT(!pass.Compute || (pass.ColorCount == 0 && !pass.DepthTarget), "Compute passes write through Write(), not targets");
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
		for (const BufferResource& r : m_buffers)
		{
			if (!r.Imported && r.Buffer)
			{
				m_pool.Release(r.Buffer);
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

		// Indexed: the observer may add passes, which can reallocate m_passes and m_uses
		const u32 first = m_passes[passIndex].FirstUse;
		const u32 count = m_passes[passIndex].UseCount;
		for (u32 i = first; i < first + count; ++i)
		{
			const Use use = m_uses[i];
			if (use.Access == Access::ShaderRead || use.IsBuffer) continue;

			const RGTexture written
			{
				.Index = use.Index,
				.Version = static_cast<u16>(use.Version + 1)
			};
			m_observer->OnWrite(*this, m_resources[use.Index].Name, written);
		}
		m_notifying = false;
	}

	void RenderGraph::Inspect()
	{
		// Resized, not cleared, so each pass's Writes keeps its capacity across frames
		m_inspection.Passes.resize(m_passes.size());
		for (const auto& [pass, info] : std::views::zip(m_passes, m_inspection.Passes))
		{
			info.Name = pass.Name;
			info.Writes.clear();
			for (const Use& use : UsesOf(pass))
			{
				if (use.Access == Access::ShaderRead || use.IsBuffer) continue;

				const Resource& r = m_resources[use.Index];
				info.Writes.push_back({ .Name = r.Name, .Version = static_cast<u16>(use.Version + 1), .Texture = r.Texture });
			}
		}
	}
}
