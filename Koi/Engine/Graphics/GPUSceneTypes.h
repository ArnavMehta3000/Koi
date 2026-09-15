#pragma once
#include "Koi/Engine/Core/Types.h"
#include "Koi/Engine/Math/Math.h"

namespace koi
{
	// CBV, one per frame-in-flight
	struct  FrameConstants
	{
		Matrix  ViewProjection;
		Vector4 CameraPosition;       // .xyz used, .w unused
		u32     MaterialBufferIndex;  // the one material table, StructuredBuffer<GPUMaterialData>
		u32     _Pad[3];
	};

	// StructuredBuffer element, one per drawn entity. Rebuilt every frame
	struct alignas(16) PerObjectData
	{
		Matrix World;
		Matrix NormalMatrix; // transpose(inverse(World)); shader uses the upper 3x3
	};

	// StructuredBuffer element, one per light. Rebuilt every frame
	struct alignas(16) GPULight
	{
		Vector3 Position;   // world position (point lights)
		u32     Type;       // LightType: 0 = directional, 1 = point
		Vector3 Direction;  // world forward (directional lights)
		f32     Range;      // point-light attenuation cutoff
		Vector3 Color;      // linear RGB
		f32     Intensity;

		Matrix  ShadowMatrix;    // world -> shadow clip
		u32     ShadowMapIndex;  // INVALID_DESCRIPTOR when this light casts no shadow
		f32     ShadowBias;
		u32     _Pad[2];
	};

	// StructuredBuffer element, one per material
	struct alignas(16) GPUMaterialData
	{
		Vector4 BaseColorFactor;
		Vector4 EmissiveFactor; // .xyz used, .w unused
		f32     MetallicFactor;
		f32     RoughnessFactor;
		f32     NormalScale;
		f32     OcclusionStrength;
		f32     AlphaCutoff;
		u32     BaseColorIndex;
		u32     MetallicRoughnessIndex;
		u32     NormalIndex;
		u32     OcclusionIndex;
		u32     EmissiveIndex;
		u32     _Pad[2];
	};

	// Root constants for the full-screen deferred lighting pass. Pushed directly
	// (28 u32 <= MAX_PUSH_CONSTANT_U32S), so no per-frame CBV is needed. Layout must
	// match the b0 cbuffer in LightingPS.hlsl (float4-aligned groups, no float3 packing).
	struct LightingRootConstants
	{
		Matrix  InvViewProjection;      // reconstructs world pos from depth
		Vector4 CameraPosition;         // .xyz used
		u32     AlbedoIndex;            // G-buffer SRVs, in the global heap
		u32     NormalIndex;
		u32     MaterialIndex;
		u32     DepthIndex;
		u32     LightBufferIndex;       // StructuredBuffer<GPULight>
		u32     LightCount;
		u32     ShadowSamplerIndex;     // comparison sampler, in the global sampler heap
		u32     _Pad;
	};

	// Root constants for the debug line pass. Must match the b0 cbuffer in DebugLineVS.hlsl
	struct DebugDrawRootConstants
	{
		Matrix ViewProjection;
		u32    VertexBufferIndex;  // StructuredBuffer<DebugVertex>
		u32    _Pad[3];
	};

#pragma warning(disable : 4324) // structure was padded due to alignment specifier
	struct alignas(16) DrawRootConstants
	{
	    u32 FrameConstantsIndex;   // which CBV, in the global heap
	    u32 ObjectBufferIndex;     // which StructuredBuffer<PerObjectData>, in the global heap
	    u32 ObjectIndex;           // which element inside that buffer
	    u32 MaterialIndex;         // which slot in the material table
	    u32 VertexBufferIndex;     // which StructuredBuffer<Vertex>
	    u32 SamplerIndex;          // which sampler, in the global sampler heap
	    u32 VertexOffset;          // added to SV_VertexID to index the shared vertex buffer
	};
#pragma warning(default : 4324)
}
