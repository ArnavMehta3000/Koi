struct VSOutput
{
	float4 Position : SV_Position;
	float2 NDC      : TEXCOORD0;
};

// Fullscreen triangle on the far plane (z = w)
VSOutput VSMain(uint id : SV_VertexID)
{
	float2 uv = float2(
		(id & 1) ? 2.0 : 0.0,
		(id & 2) ? 2.0 : 0.0);

	VSOutput output;
	output.NDC      = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
	output.Position = float4(output.NDC, 1.0, 1.0);
	return output;
}
