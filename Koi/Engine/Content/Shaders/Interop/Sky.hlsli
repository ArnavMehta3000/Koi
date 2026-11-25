#ifndef _KOI_INTEROP_SKY_HLSLI_
#define _KOI_INTEROP_SKY_HLSLI_
#include "Interop.hlsli"

KOI_INTEROP_BEGIN

struct SkyRootConstants
{
	Matrix          InvViewProjection;  // clip -> world, to rebuild the view ray
	Vector3         CameraPosition;
	f32             Rotation;           // around world up, radians
	Vector3         Radiance;           // color * intensity, multiplies the image
	DescriptorIndex ImageIndex;         // equirectangular Texture2D, white without an image
	DescriptorIndex SamplerIndex;       // wraps U, clamps V
	u32             _Pad0;
	u32             _Pad1;
	u32             _Pad2;
};
KOI_CHECK_SIZE(SkyRootConstants, 112);

KOI_INTEROP_END

#endif
