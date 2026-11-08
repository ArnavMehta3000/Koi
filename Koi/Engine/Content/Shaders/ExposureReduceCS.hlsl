#include "Interop/PostProcess.hlsli"

ConstantBuffer<ExposureRootConstants> g_root : register(b0);

groupshared float gs_weighted[HISTOGRAM_BIN_COUNT];

[numthreads(HISTOGRAM_BIN_COUNT, 1, 1)]
void CSMain(uint groupIndex : SV_GroupIndex)
{
	StructuredBuffer<uint>    histogram = ResourceDescriptorHeap[g_root.HistogramIndex];
	RWStructuredBuffer<float> exposure  = ResourceDescriptorHeap[g_root.ExposureIndex];

	const uint count = histogram[groupIndex];
	gs_weighted[groupIndex] = float(count) * groupIndex;
	GroupMemoryBarrierWithGroupSync();

	[unroll]
	for (uint stride = HISTOGRAM_BIN_COUNT / 2; stride > 0; stride >>= 1)
	{
		if (groupIndex < stride)
		{
			gs_weighted[groupIndex] += gs_weighted[groupIndex + stride];
		}
		GroupMemoryBarrierWithGroupSync();
	}

	if (groupIndex == 0)
	{
		// Thread 0 read bin 0, so count is the black pixels
		const float litPixels = max(g_root.PixelCount - float(count), 1.0);
		const float meanBin   = max(gs_weighted[0] / litPixels, 1.0);
		const float logLum    = (meanBin - 1.0) / (HISTOGRAM_BIN_COUNT - 2) * g_root.LogLuminanceRange + g_root.MinLogLuminance;

		exposure[0] = g_root.KeyValue / exp2(logLum) * exp2(g_root.ExposureCompensation);
	}
}
