#include "MeshShaderCommon.hlsl"

void PSMain(float4 pos : SV_Position, float2 uv : TEXCOORD0)
{
	ConstantBuffer<FrameConstants>    frame     = ResourceDescriptorHeap[g_root.FrameConstantsIndex];
	StructuredBuffer<GPUMaterialData> materials = ResourceDescriptorHeap[frame.MaterialBufferIndex];
	GPUMaterialData mat = materials[g_root.MaterialIndex];

	Texture2D    baseColorTex = ResourceDescriptorHeap[mat.BaseColorIndex];
	SamplerState samp         = SamplerDescriptorHeap[g_root.SamplerIndex];

	clip(baseColorTex.Sample(samp, uv).a * mat.BaseColorFactor.a - mat.AlphaCutoff);
}
