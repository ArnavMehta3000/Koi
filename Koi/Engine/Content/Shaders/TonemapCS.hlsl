#include "Interop/PostProcess.hlsli"
#include "Tonemapping.hlsl"

ConstantBuffer<TonemapRootConstants> g_root : register(b0);

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
	Texture2D<float4>   hdrTex = ResourceDescriptorHeap[g_root.HDRTextureIndex];
	RWTexture2D<float4> ldrTex = ResourceDescriptorHeap[g_root.LDRTextureIndex];

	uint width, height;
	ldrTex.GetDimensions(width, height);
	if (id.x >= width || id.y >= height)
		return;

	float3 hdr = hdrTex.Load(int3(id.xy, 0)).rgb;
	ldrTex[id.xy] = float4(ACES(hdr), 1.0);
}
