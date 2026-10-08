#pragma once
#include "Koi/Engine/Graphics/RenderGraph/RGHandles.h"
#include "Koi/Engine/Math/MathTypes.h"

namespace koi
{
	class RenderGraph;

	enum class LoadOp : u8
	{
		Load,   // keep what is there
		Clear,  // clear to the attachment's clear value before the pass runs
	};

	// Handed to a pass's setup callback
	class RGBuilder : NonCopyable
	{
		friend class RenderGraph;
	public:
		// Sampled from any shader stage. Invalid handles are ignored, so optional inputs need no branch
		void Read(RGTexture texture);

		// Bound as a render target; returns the version the pass produces
		[[nodiscard]] RGTexture ColorTarget(RGTexture texture, LoadOp load = LoadOp::Load, const Color& clear = Color(0.0f, 0.0f, 0.0f, 1.0f));
		[[nodiscard]] RGTexture DepthTarget(RGTexture texture, LoadOp load = LoadOp::Load, f32 clearDepth = 1.0f);

	private:
		RGBuilder(RenderGraph& graph, u32 passIndex) : m_graph(graph), m_passIndex(passIndex) {}

	private:
		RenderGraph& m_graph;
		u32          m_passIndex;
	};
}
