module;
#include "Core/Config.h"
#include <string_view>
#include <unordered_map>
module Koi.Graphics.ShaderLoader;

import Koi.Core.Logger;
import Koi.Core.VirtualFileSystem;

#if KOI_WITH_EDITOR
	import Koi.Graphics.ShaderCompiler;
#endif
#include "Core/Logger.h"

namespace koi
{
	std::vector<byte> LoadShaderBytecode(std::string_view path)
	{
		if (VirtualFileSystem::SplitVirtualPath(path))
		{
			if (auto data = VirtualFileSystem::Get().Read(path))
			{
				return std::move(*data);
			}
			KOI_LOG_ERROR("Failed to load shader '{}'", path);
			return {};
		}

		std::ifstream file(std::filesystem::path(path), std::ios::binary | std::ios::ate);
		if (!file.is_open())
		{
			KOI_LOG_ERROR("Failed to load shader '{}'", path);
			return {};
		}

		std::vector<byte> result(static_cast<size_t>(file.tellg()));
		file.seekg(0, std::ios::beg);
		file.read(reinterpret_cast<char*>(result.data()), result.size());
		return result;
	}

	ShaderType InferShaderTypeFromName(std::string_view path)
	{
		auto slash = path.find_last_of("/\\");
		std::string_view stem = slash == std::string_view::npos ? path : path.substr(slash + 1);
		auto dot = stem.find_last_of('.');
		if (dot != std::string_view::npos) stem = stem.substr(0, dot);

		if (stem.size() < 2) return ShaderType::Unknown;
		std::string_view suffix = stem.substr(stem.size() - 2);
		if (suffix == "VS") return ShaderType::Vertex;
		if (suffix == "PS") return ShaderType::Pixel;
		if (suffix == "CS") return ShaderType::Compute;
		if (suffix == "HS") return ShaderType::Hull;
		if (suffix == "DS") return ShaderType::Domain;
		if (suffix == "GS") return ShaderType::Geometry;
		return ShaderType::Unknown;
	}

#if KOI_WITH_EDITOR
	namespace
	{
		std::mutex g_shaderCacheMutex;
		std::unordered_map<std::string, std::vector<byte>> g_shaderCache;
	}

	std::vector<byte> LoadShader(std::string_view virtualPathNoExt)
	{
		{
			std::lock_guard lock(g_shaderCacheMutex);
			if (auto it = g_shaderCache.find(std::string(virtualPathNoExt)); it != g_shaderCache.end())
				return it->second;
		}

		const ShaderType type = InferShaderTypeFromName(virtualPathNoExt);
		if (type == ShaderType::Unknown)
		{
			KOI_LOG_ERROR("LoadShader: cannot infer type from '{}' (expected *VS/*PS/*CS/*HS/*DS/*GS)", virtualPathNoExt);
			return {};
		}

		const std::string hlslPath = std::string(virtualPathNoExt) + ".hlsl";
		const auto resolved = VirtualFileSystem::Get().TryResolveToDisk(hlslPath);
		if (!resolved || !std::filesystem::exists(*resolved))
		{
			KOI_LOG_ERROR("LoadShader: source '{}' not found on disk", hlslPath);
			return {};
		}

		ShaderCompiler::Args args{ .Type = type };

		auto blob = ShaderCompiler::Get().CompileFromFile(*resolved, args);
		if (!blob || blob->GetBufferSize() == 0)
		{
			KOI_LOG_ERROR("LoadShader: compile failed for '{}'", hlslPath);
			return {};
		}

		std::vector<byte> bytes(
			static_cast<const byte*>(blob->GetBufferPointer()),
			static_cast<const byte*>(blob->GetBufferPointer()) + blob->GetBufferSize());

		{
			std::lock_guard lock(g_shaderCacheMutex);
			g_shaderCache[std::string(virtualPathNoExt)] = bytes;
		}
		return bytes;
	}

	void InvalidateShaderCache()
	{
		std::lock_guard lock(g_shaderCacheMutex);
		g_shaderCache.clear();
		KOI_LOG_INFO("Shader cache invalidated");
	}
#else
	std::vector<byte> LoadShader(std::string_view virtualPathNoExt)
	{
		const std::string csoPath = std::string(virtualPathNoExt) + ".cso";
		return LoadShaderBytecode(csoPath);
	}

	void InvalidateShaderCache() {}
#endif
}
