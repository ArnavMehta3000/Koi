struct VSOutput
{
	float4 Position : SV_Position;
	float4 Color    : COLOR0;
};

float4 PSMain(VSOutput input) : SV_Target
{
	return input.Color;
}
