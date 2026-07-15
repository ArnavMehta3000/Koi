struct VSOutput
{
	float4 Position : SV_Position;
	float2 UV       : TEXCOORD0;
};

// Generate oversized fullscreen triangle
//  id=0  (-1,  1) top-left
//  id=1  ( 3,  1) far right  (off-screen)
//  id=2  (-1, -3) far bottom (off-screen)
//
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
