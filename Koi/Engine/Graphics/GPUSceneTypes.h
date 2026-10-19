#pragma once
#include "Koi/Engine/Content/Shaders/Interop/Lighting.hlsli"
#include "Koi/Engine/Content/Shaders/Interop/Mesh.hlsli"
#include "Koi/Engine/Core/Types.h"
#include "Koi/Engine/Math/MathTypes.h"

namespace koi
{
	// CBV for the infinite grid pass. Must match the GridConstants cbuffer in GridPS.hlsl
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
		f32     _Pad[3];
	};

	// Root constants for the target capture pass. Must match the b0 cbuffer in DebugVisualizeCS.hlsl
	struct CaptureRootConstants
	{
		u32 SourceIndex;
		u32 PreviewIndex;
		u32 Mode;         // CaptureMode
		u32 ChannelMask;  // bit per RGBA channel
		f32 RangeMin;
		f32 RangeMax;
		f32 Near;
		f32 Far;
	};
}
