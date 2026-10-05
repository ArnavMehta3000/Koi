struct VSOutput
{
	float4 Position : SV_Position;
	float2 NDC      : TEXCOORD0;
};

// Oversized fullscreen triangle; the pixel shader rebuilds a view ray from NDC
VSOutput VSMain(uint id : SV_VertexID)
{
	float2 uv = float2(
		(id & 1) ? 2.0 : 0.0,
		(id & 2) ? 2.0 : 0.0);

	float2 ndc = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);

	VSOutput output;
	output.Position = float4(ndc, 0.0, 1.0);
	output.NDC      = ndc;

	return output;
}
