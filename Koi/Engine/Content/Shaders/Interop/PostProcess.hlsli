#ifndef _KOI_INTEROP_POST_PROCESS_HLSLI_
#define _KOI_INTEROP_POST_PROCESS_HLSLI_
#include "Interop.hlsli"

KOI_INTEROP_BEGIN

static const u32 TONEMAPPER_ACES        = 0;
static const u32 TONEMAPPER_AGX         = 1;
static const u32 TONEMAPPER_PBR_NEUTRAL = 2;
static const u32 TONEMAPPER_HABLE       = 3;
static const u32 TONEMAPPER_REINHARD    = 4;
static const u32 TONEMAPPER_NONE        = 5;

struct TonemapRootConstants
{
	DescriptorIndex HDRTextureIndex;  // SRV
	DescriptorIndex LDRTextureIndex;  // UAV
};
KOI_CHECK_SIZE(TonemapRootConstants, 8);

struct BlitRootConstants
{
	DescriptorIndex SourceIndex;
};
KOI_CHECK_SIZE(BlitRootConstants, 4);

// Root constants for the target capture pass
struct CaptureRootConstants
{
	DescriptorIndex SourceIndex;
	DescriptorIndex PreviewIndex;  // UAV
	u32             Mode;          // CaptureMode: 0 = color, 1 = linear depth
	u32             ChannelMask;   // bit per RGBA channel
	f32             RangeMin;
	f32             RangeMax;
	f32             Near;
	f32             Far;
};
KOI_CHECK_SIZE(CaptureRootConstants, 32);

KOI_INTEROP_END

#endif
