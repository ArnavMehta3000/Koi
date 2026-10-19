#ifndef _KOI_INTEROP_GRID_HLSLI_
#define _KOI_INTEROP_GRID_HLSLI_
#include "Interop.hlsli"

KOI_INTEROP_BEGIN

// CBV for the infinite grid pass
struct alignas(16) GridConstants
{
	Matrix  ViewProjection;     // world -> clip, for the SV_Depth the occlusion test needs
	Matrix  InvViewProjection;  // clip -> world, to rebuild the view ray per pixel
	Vector4 CameraPosition;     // .xyz used, ray origin
	Vector4 ThinColor;          // minor cell lines, .w is peak opacity
	Vector4 ThickColor;         // every ThickEvery-th line
	Vector4 AxisXColor;         // the x axis (world z == 0)
	Vector4 AxisZColor;         // the z axis (world x == 0)
	f32     CellSize;           // world units between minor lines
	f32     ThickEvery;         // minor lines per major line
	f32     FadeStart;          // distance where the grid starts fading out
	f32     FadeEnd;            // distance where it is fully gone
	f32     MajorLineWidth;     // major/axis line width in pixels; minor lines are always 1
	f32     _Pad0;
	f32     _Pad1;
	f32     _Pad2;
};
KOI_CHECK_SIZE(GridConstants, 240);

struct GridRootConstants
{
	DescriptorIndex GridConstantsIndex;  // ConstantBuffer<GridConstants>
};
KOI_CHECK_SIZE(GridRootConstants, 4);

KOI_INTEROP_END

#endif
