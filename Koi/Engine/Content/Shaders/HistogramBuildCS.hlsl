#include "Interop/PostProcess.hlsli"

ConstantBuffer<HistogramRootConstants> g_root : register(b0);

groupshared uint gs_bins[HISTOGRAM_BIN_COUNT];

uint LuminanceToBin(float3 color)
{
	const float logLum = log2(max(dot(color, float3(0.2126, 0.7152, 0.0722)), 1e-10));
	if (logLum < g_root.MinLogLuminance)
		return 0;

	const float t = saturate((logLum - g_root.MinLogLuminance) * g_root.InvLogLuminanceRange);
	return uint(t * (HISTOGRAM_BIN_COUNT - 2) + 1.0);
}

// 16x16 threads, one per bin, so each thread also clears and flushes one shared bin
[numthreads(16, 16, 1)]
void CSMain(uint3 id : SV_DispatchThreadID, uint groupIndex : SV_GroupIndex)
{
	Texture2D<float4>        hdrTex    = ResourceDescriptorHeap[g_root.HDRTextureIndex];
	RWStructuredBuffer<uint> histogram = ResourceDescriptorHeap[g_root.HistogramIndex];

	gs_bins[groupIndex] = 0;
	GroupMemoryBarrierWithGroupSync();

	uint width, height;
	hdrTex.GetDimensions(width, height);
	if (id.x < width && id.y < height)
	{
		InterlockedAdd(gs_bins[LuminanceToBin(hdrTex.Load(int3(id.xy, 0)).rgb)], 1);
	}
	GroupMemoryBarrierWithGroupSync();

	if (gs_bins[groupIndex] != 0)
	{
		InterlockedAdd(histogram[groupIndex], gs_bins[groupIndex]);
	}
}
