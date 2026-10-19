#ifndef _KOI_INTEROP_MESH_HLSLI_
#define _KOI_INTEROP_MESH_HLSLI_
#include "Interop.hlsli"

KOI_INTEROP_BEGIN

// StructuredBuffer element of every mesh's one interleaved vertex stream
struct Vertex
{
	Vector3 Position;
	Vector3 Normal;
	Vector4 Tangent;
	Vector2 UV;
};
KOI_CHECK_SIZE(Vertex, 48);

// CBV, one per frame-in-flight
struct FrameConstants
{
	Matrix          ViewProjection;
	Vector4         CameraPosition;       // .xyz used, .w unused
	DescriptorIndex MaterialBufferIndex;  // the one material table, StructuredBuffer<GPUMaterialData>
	u32             _Pad0;
	u32             _Pad1;
	u32             _Pad2;
};
KOI_CHECK_SIZE(FrameConstants, 96);

// StructuredBuffer element, one per drawn entity. Rebuilt every frame
struct alignas(16) PerObjectData
{
	Matrix World;
	Matrix NormalMatrix;  // transpose(inverse(World)); shader uses the upper 3x3
};
KOI_CHECK_SIZE(PerObjectData, 128);

// StructuredBuffer element, one per material
struct alignas(16) GPUMaterialData
{
	Vector4         BaseColorFactor;
	Vector4         EmissiveFactor;  // .xyz used, .w unused
	f32             MetallicFactor;
	f32             RoughnessFactor;
	f32             NormalScale;
	f32             OcclusionStrength;
	f32             AlphaCutoff;
	DescriptorIndex BaseColorIndex;
	DescriptorIndex MetallicRoughnessIndex;
	DescriptorIndex NormalIndex;
	DescriptorIndex OcclusionIndex;
	DescriptorIndex EmissiveIndex;
	u32             _Pad0;
	u32             _Pad1;
};
KOI_CHECK_SIZE(GPUMaterialData, 80);

// Root constants for every mesh draw (GBuffer, Shadow)
struct DrawRootConstants
{
	DescriptorIndex FrameConstantsIndex;  // ConstantBuffer<FrameConstants>
	DescriptorIndex ObjectBufferIndex;    // StructuredBuffer<PerObjectData>
	u32             ObjectIndex;          // element inside that buffer
	u32             MaterialIndex;        // slot in the material table
	DescriptorIndex VertexBufferIndex;    // StructuredBuffer<Vertex>
	DescriptorIndex SamplerIndex;         // in the global sampler heap
	u32             VertexOffset;         // added to SV_VertexID to index the shared vertex buffer
};
KOI_CHECK_SIZE(DrawRootConstants, 28);

KOI_INTEROP_END

#endif
