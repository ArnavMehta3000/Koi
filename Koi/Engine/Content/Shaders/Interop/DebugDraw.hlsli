#ifndef _KOI_INTEROP_DEBUG_DRAW_HLSLI_
#define _KOI_INTEROP_DEBUG_DRAW_HLSLI_
#include "Interop.hlsli"

KOI_INTEROP_BEGIN

struct DebugVertex
{
	Vector3 Position;
	u32     Color;  // RGBA8, low byte is red
};
KOI_CHECK_SIZE(DebugVertex, 16);

// Root constants for the debug line pass
struct DebugDrawRootConstants
{
	Matrix          ViewProjection;
	DescriptorIndex VertexBufferIndex;  // StructuredBuffer<DebugVertex>
	u32             _Pad0;
	u32             _Pad1;
	u32             _Pad2;
};
KOI_CHECK_SIZE(DebugDrawRootConstants, 80);

KOI_INTEROP_END

#endif
