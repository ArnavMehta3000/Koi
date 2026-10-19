#ifndef _MESH_SHADER_COMMON_HLSL_
#define _MESH_SHADER_COMMON_HLSL_

#include "Interop/Mesh.hlsli"

ConstantBuffer<DrawRootConstants> g_root : register(b0);

static const uint INVALID_DESCRIPTOR = 0xFFFFFFFFu;

#endif // _MESH_SHADER_COMMON_HLSL_
