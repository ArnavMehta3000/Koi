#pragma once
#include "Koi/Engine/Core/Types.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/IO/VirtualFileSystem.h"
#include "Koi/Engine/Graphics/Types.h"
#include <filesystem>
#include <fstream>
#include <string_view>
#include <vector>

namespace koi
{
	// Reads a CSO, accepts VFS path
	[[nodiscard]] std::vector<byte> LoadShaderBytecode(std::string_view path);

	// Infer ShaderType from a virtual path's filename suffix:
	//   *VS -> Vertex, *PS -> Pixel, *CS -> Compute, *HS -> Hull,
	//   *DS -> Domain, *GS -> Geometry.
	[[nodiscard]] ShaderType InferShaderTypeFromName(std::string_view path);

	// Mode-agnostic shader load. Pass a virtual path WITHOUT extension:
	//   auto vs = LoadShader("engine://Shaders/TonemapVS");
	//
	// Editor builds:  resolves "<path>.hlsl" via VFS, compiles via DXC,
	//                 returns DXIL bytes (cached in memory until Invalidate).
	// Runtime builds: resolves "<path>.cso" via VFS, returns the bytes
	[[nodiscard]] std::vector<byte> LoadShader(std::string_view virtualPathNoExt);

	// Drops every in-memory compiled-shader entry, so the next LoadShader
	// call recompiles from source. Used by the editor's F5 hot-reload.
	// No-op in runtime builds.
	void InvalidateShaderCache();
}
