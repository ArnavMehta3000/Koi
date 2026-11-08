#include "Interop/PostProcess.hlsli"

ConstantBuffer<HistogramRootConstants> g_root : register(b0);

[numthreads(HISTOGRAM_BIN_COUNT, 1, 1)]
void CSMain(uint groupIndex : SV_GroupIndex)
{
	RWStructuredBuffer<uint> histogram = ResourceDescriptorHeap[g_root.HistogramIndex];
	histogram[groupIndex] = 0;
}
