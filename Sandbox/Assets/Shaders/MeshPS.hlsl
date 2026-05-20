struct PSInput
{
    float4 Position    : SV_Position;
    float3 WorldNormal : NORMAL;
    float2 UV          : TEXCOORD0;
};

float4 PSMain(PSInput input) : SV_Target
{
    float3 N = normalize(input.WorldNormal);
    float3 L = normalize(float3(0.4f, 0.8f, 0.3f));

    float  ndotl     = saturate(dot(N, L));
    float3 baseColor = float3(0.8f, 0.8f, 0.8f);
    float3 ambient   = 0.15f * baseColor;
    float3 color     = ambient + baseColor * ndotl;

    return float4(color, 1.0f);
}
