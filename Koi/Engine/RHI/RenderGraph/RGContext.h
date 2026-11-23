#pragma once
#include "Koi/Engine/RHI/PipelineCache.h"
#include "Koi/Engine/RHI/RenderGraph/RGHandles.h"
#include "Koi/Engine/RHI/CommandList.h"
#include <cassert>
#include <type_traits>

namespace koi
{
	class RenderGraph;

	// A struct that fits the root constant slot as-is
	template <typename T>
	concept RootConstantBlock = std::is_trivially_copyable_v<T>
		&& sizeof(T) % sizeof(u32) == 0
		&& sizeof(T) / sizeof(u32) <= MAX_PUSH_CONSTANT_U32S;

	// What every pass kind can do
	class PassContext : NonCopyable
	{
	public:
		[[nodiscard]] TextureHandle Texture(RGTexture texture) const;
		[[nodiscard]] DescriptorIndex SRV(RGTexture texture) const;
		[[nodiscard]] DescriptorIndex UAV(RGTexture texture) const;
		[[nodiscard]] BufferHandle Buffer(RGBuffer buffer) const;
		[[nodiscard]] DescriptorIndex SRV(RGBuffer buffer) const;
		[[nodiscard]] DescriptorIndex UAV(RGBuffer buffer) const;

		// src must be declared with CopyFrom; dst is a buffer the graph does not track
		void CopyBuffer(BufferHandle dst, u64 dstOffset, RGBuffer src, u64 size);

		template <RootConstantBlock T>
		inline void SetRootConstants(const T& constants)
		{
			m_cmd.SetRootConstants(&constants, sizeof(T) / sizeof(u32));
		}

	protected:
		PassContext(CommandList& cmd, const RenderGraph& graph) : m_cmd(cmd), m_graph(graph) {}

		[[nodiscard]] bool Bind(const std::expected<BoundPipeline, PipelineError>& pipeline);

	protected:
		CommandList&       m_cmd;
		const RenderGraph& m_graph;
		BoundPipeline      m_bound;
	};

	// Raster pass capabilities
	class RasterContext : public PassContext
	{
	public:
		RasterContext(CommandList& cmd, const RenderGraph& graph, const TargetFormats& targets)
			: PassContext(cmd, graph), m_targets(targets) {}

		// Built against this pass's target formats. False if it failed to build: skip the draw
		[[nodiscard]] bool SetPipeline(const RasterPipeline& pipeline);

		inline void SetIndexBuffer(BufferHandle buffer) { m_cmd.SetIndexBuffer(buffer); }

		inline void Draw(u32 vertexCount, u32 instanceCount = 1, u32 startVertex = 0, u32 startInstance = 0)
		{
			m_cmd.Draw(vertexCount, instanceCount, startVertex, startInstance);
		}

		inline void DrawIndexed(u32 indexCount, u32 instanceCount = 1, u32 startIndex = 0, i32 vertexOffset = 0, u32 startInstance = 0)
		{
			m_cmd.DrawIndexed(indexCount, instanceCount, startIndex, vertexOffset, startInstance);
		}

	private:
		const TargetFormats& m_targets;
	};

	// Compute pass capabilities
	class ComputeContext : public PassContext
	{
	public:
		ComputeContext(CommandList& cmd, const RenderGraph& graph) : PassContext(cmd, graph) {}

		// False if it failed to build: skip the dispatch
		[[nodiscard]] bool SetPipeline(const ComputePipeline& pipeline);

		inline void Dispatch(u32 groupsX, u32 groupsY = 1, u32 groupsZ = 1) { m_cmd.Dispatch(groupsX, groupsY, groupsZ); }

		// The bound shader's [numthreads], from its reflection
		[[nodiscard]] inline const std::array<u32, 3>& GetThreadGroup() const noexcept { return m_bound.Info.ThreadGroup; }
	};

	// Pass kinds. Each names the context its execute callback receives
	struct RasterPass  { using Context = RasterContext;  };
	struct ComputePass { using Context = ComputeContext; };

	template <typename T>
	concept PassKind = requires { typename T::Context; };
}
