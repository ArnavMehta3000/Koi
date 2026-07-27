struct VSOutput
{
	float4 Position : SV_Position;
	float2 UV       : TEXCOORD0;
};

// Oversized full-screen triangle generated from SV_VertexID (Draw(3), no vertex buffer):
//  id=0 -> uv(0,0) -> pos(-1,  1)  top-left
//  id=1 -> uv(2,0) -> pos( 3,  1)  far right  (off-screen)
//  id=2 -> uv(0,2) -> pos(-1, -3)  far bottom (off-screen)
// UV origin is top-left (0,0)
VSOutput VSMain(uint id : SV_VertexID)
{
	VSOutput output;

	float2 uv = float2(
		(id & 1) ? 2.0 : 0.0,
		(id & 2) ? 2.0 : 0.0
	);

	output.Position = float4(
		uv.x * 2.0 - 1.0,
		1.0 - uv.y * 2.0,
		0.0,
		1.0);

	output.UV = uv;

	return output;
}
