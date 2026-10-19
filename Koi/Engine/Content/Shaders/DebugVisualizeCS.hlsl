#include "Interop/PostProcess.hlsli"

ConstantBuffer<CaptureRootConstants> g_root : register(b0);

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
	Texture2D<float4>   source  = ResourceDescriptorHeap[g_root.SourceIndex];
	RWTexture2D<float4> preview = ResourceDescriptorHeap[g_root.PreviewIndex];

	uint width, height;
	preview.GetDimensions(width, height);
	if (id.x >= width || id.y >= height)
		return;

	float4 value = source.Load(int3(id.xy, 0));

	if (g_root.Mode == 1)
	{
		// Undo the perspective depth curve, then spread [Near, Far] over [0, 1]
		const float nearZ = g_root.Near;
		const float farZ  = g_root.Far;
		const float viewZ = nearZ * farZ / (farZ - value.r * (farZ - nearZ));
		value = (viewZ - nearZ) / (farZ - nearZ);
	}

	value = saturate((value - g_root.RangeMin) / max(g_root.RangeMax - g_root.RangeMin, 1e-5));

	const float4 mask = float4((g_root.ChannelMask >> uint4(0, 1, 2, 3)) & 1);

	// A single channel reads better as grayscale than as a tinted image
	if (countbits(g_root.ChannelMask) == 1)
	{
		preview[id.xy] = float4(dot(value, mask).xxx, 1.0);
		return;
	}

	preview[id.xy] = float4(value.rgb * mask.rgb, 1.0);
}
