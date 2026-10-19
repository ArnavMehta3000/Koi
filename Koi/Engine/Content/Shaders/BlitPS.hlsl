#include "Interop/PostProcess.hlsli"

ConstantBuffer<BlitRootConstants> g_root : register(b0);

float4 PSMain(float4 pos : SV_Position) : SV_Target
{
	Texture2D<float4> source = ResourceDescriptorHeap[g_root.SourceIndex];
	return source.Load(int3(pos.xy, 0));
}
