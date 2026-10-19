#ifndef _KOI_INTEROP_LIGHTING_HLSLI_
#define _KOI_INTEROP_LIGHTING_HLSLI_
#include "Interop.hlsli"

KOI_INTEROP_BEGIN

// StructuredBuffer element, one per light. Rebuilt every frame
struct alignas(16) GPULight
{
	Vector3 Position;   // world position (point lights)
	u32     Type;       // LightType: 0 = directional, 1 = point
	Vector3 Direction;  // world forward (directional lights)
	f32     Range;      // point-light attenuation cutoff
	Vector3 Color;      // linear RGB
	f32     Intensity;

	Matrix          ShadowMatrix;    // world -> shadow clip
	DescriptorIndex ShadowMapIndex;  // INVALID_DESCRIPTOR when this light casts no shadow
	f32             ShadowBias;
	u32             _Pad0;
	u32             _Pad1;
};
KOI_CHECK_SIZE(GPULight, 128);

// Root constants for the full-screen deferred lighting pass
struct LightingRootConstants
{
	Matrix          InvViewProjection;   // reconstructs world pos from depth
	Vector4         CameraPosition;      // .xyz used
	DescriptorIndex AlbedoIndex;         // G-buffer SRVs, in the global heap
	DescriptorIndex NormalIndex;
	DescriptorIndex MaterialIndex;
	DescriptorIndex DepthIndex;
	DescriptorIndex LightBufferIndex;    // StructuredBuffer<GPULight>
	u32             LightCount;
	DescriptorIndex ShadowSamplerIndex;  // comparison sampler, in the global sampler heap
	u32             _Pad;
};
KOI_CHECK_SIZE(LightingRootConstants, 112);

KOI_INTEROP_END

#endif
