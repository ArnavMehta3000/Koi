#ifndef _MESH_PUSH_CONSTANTS_HLSL_
#define _MESH_PUSH_CONSTANTS_HLSL_

cbuffer PushConstants : register(b0)
{
	uint FrameConstantsIndex;
	uint ObjectBufferIndex;
	uint ObjectIndex;
	uint MaterialBufferIndex;
	uint MaterialIndex;
	uint VertexBufferIndex;
	uint SamplerIndex;
};

#endif // _MESH_PUSH_CONSTANTS_HLSL_
