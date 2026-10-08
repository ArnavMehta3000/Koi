cbuffer PushConstants : register(b0)
{
	uint  SourceIndex;
	uint  Mode;         // 0 = color, 1 = linear depth
	uint  ChannelMask;  // bit per RGBA channel
	float RangeMin;
	float RangeMax;
	float Near;
	float Far;
};

float4 PSMain(float4 pos : SV_Position) : SV_Target
{
	Texture2D<float4> source = ResourceDescriptorHeap[SourceIndex];
	float4 value = source.Load(int3(pos.xy, 0));

	if (Mode == 1)
	{
		// Undo the perspective depth curve, then spread [Near, Far] over [0, 1]
		const float viewZ = Near * Far / (Far - value.r * (Far - Near));
		value = (viewZ - Near) / (Far - Near);
	}

	value = saturate((value - RangeMin) / max(RangeMax - RangeMin, 1e-5));

	const float4 mask = float4((ChannelMask >> uint4(0, 1, 2, 3)) & 1);

	// A single channel reads better as grayscale than as a tinted image
	if (countbits(ChannelMask) == 1)
	{
		
		return float4(dot(value, mask).xxx, 1.0);
	}

	return float4(value.rgb * mask.rgb, 1.0);
}
