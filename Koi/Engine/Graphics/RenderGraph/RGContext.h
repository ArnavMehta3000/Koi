#pragma once
#include "Koi/Engine/Graphics/PipelineCache.h"
#include "Koi/Engine/Graphics/RenderGraph/RGHandles.h"
#include "Koi/Engine/Graphics/RHI/CommandList.h"
#include <type_traits>

namespace koi
{
	class RenderGraph;

	// A struct that fits the root constant slot as-is
	template <typename T>
	concept RootConstantBlock = std::is_trivially_copyable_v<T>
		&& sizeof(T) % sizeof(u32) == 0
		&& sizeof(T) / sizeof(u32) <= MAX_PUSH_CONSTANT_U32S;

	// Raster pass capabilities
	class RasterContext : NonCopyable
	{
	public:
		RasterContext(CommandList& cmd, const RenderGraph& graph, const TargetFormats& targets)
			: m_cmd(cmd), m_graph(graph), m_targets(targets) {}

		[[nodiscard]] TextureHandle Texture(RGTexture texture) const;
		[[nodiscard]] DescriptorIndex SRV(RGTexture texture) const;

		// Built against this pass's target formats. False if it failed to build: skip the draw
		[[nodiscard]] bool SetPipeline(const RasterPipeline& pipeline);

		inline void SetIndexBuffer(BufferHandle buffer) { m_cmd.SetIndexBuffer(buffer); }

		template <RootConstantBlock T>
		inline void SetRootConstants(const T& constants)
		{
			m_cmd.SetRootConstants(&constants, sizeof(T) / sizeof(u32));
		}

		inline void Draw(u32 vertexCount, u32 instanceCount = 1, u32 startVertex = 0, u32 startInstance = 0)
		{
			m_cmd.Draw(vertexCount, instanceCount, startVertex, startInstance);
		}

		inline void DrawIndexed(u32 indexCount, u32 instanceCount = 1, u32 startIndex = 0, i32 vertexOffset = 0, u32 startInstance = 0)
		{
			m_cmd.DrawIndexed(indexCount, instanceCount, startIndex, vertexOffset, startInstance);
		}

	private:
		CommandList&         m_cmd;
		const RenderGraph&   m_graph;
		const TargetFormats& m_targets;
	};

	// Pass kinds. Each names the context its execute callback receives
	struct RasterPass { using Context = RasterContext; };

	template <typename T>
	concept PassKind = requires { typename T::Context; };
}
