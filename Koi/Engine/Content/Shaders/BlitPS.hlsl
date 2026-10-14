cbuffer PushConstants : register(b0)
{
	uint SourceIndex;
};

float4 PSMain(float4 pos : SV_Position) : SV_Target
{
	Texture2D<float4> source = ResourceDescriptorHeap[SourceIndex];
	return source.Load(int3(pos.xy, 0));
}
