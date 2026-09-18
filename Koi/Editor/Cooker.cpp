#include "Koi/Editor/Cooker.h"
#include "Koi/Engine/Graphics/PipelineLibrary.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Graphics/ShaderCache.h"
#include "Koi/Engine/Graphics/ShaderCompiler.h"
#include "Koi/Engine/IO/VirtualFileSystem.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <system_error>
#include <vector>

namespace koi::editor
{
	namespace
	{
		struct ShaderTally { u32 Ok = 0; u32 Total = 0; };

		static ShaderTally CookShaders(const fs::path& outRoot)
		{
			const std::vector<std::string> shaderPaths = Engine::Get().GetSubsystem<Renderer>().GetPipelineLibrary().CollectShaderPaths();

			std::error_code ec;
			ShaderTally tally;

			for (const std::string& virtualPath : shaderPaths)
			{
				tally.Total++;

				const auto split = VirtualFileSystem::SplitVirtualPath(virtualPath);
				if (!split)
				{
					KOI_LOG_ERROR("Cook: '{}' is not a virtual path", virtualPath);
					continue;
				}

				const auto [scheme, relative] = *split;
				if (scheme != "engine" && scheme != "project")
				{
					KOI_LOG_ERROR("Cook: unknown scheme '{}://' on '{}'", scheme, virtualPath);
					continue;
				}

				const ShaderType type = InferShaderTypeFromName(virtualPath);
				if (type == ShaderType::Unknown)
				{
					KOI_LOG_ERROR("Cook: cannot infer stage from '{}'", virtualPath);
					continue;
				}

				const auto source = VirtualFileSystem::Get().TryResolveToDisk(std::string(virtualPath) + ".hlsl");
				if (!source || !fs::exists(*source))
				{
					KOI_LOG_ERROR("Cook: source for '{}' not found on disk", virtualPath);
					continue;
				}

				const fs::path outDir = outRoot
					/ (scheme == "engine" ? "Engine" : "Project")
					/ fs::path(relative).parent_path();
				fs::create_directories(outDir, ec);

				ShaderCompiler::Args args{ .Type = type, .OutDir = outDir };
				auto blob = ShaderCompiler::Get().CompileFromFile(*source, args);
				if (blob && blob->GetBufferSize() > 0)
				{
					KOI_LOG_DEBUG("Cook: {} OK", virtualPath);
					tally.Ok++;
				}
				else
				{
					KOI_LOG_ERROR("Cook: {} FAILED", virtualPath);
				}
			}

			return tally;
		}

		static void CopyDir(const fs::path& src, const fs::path& dst)
		{
			if (!fs::exists(src)) return;

			std::error_code ec;

			fs::create_directories(dst, ec);
			fs::copy(src, dst, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);

			if (ec)
			{
				KOI_LOG_WARN("Cook: copy {} -> {} failed: {}", src.string(), dst.string(), ec.message());
			}
			else
			{
				KOI_LOG_DEBUG("Cook: copied {} -> {}", src.string(), dst.string());
			}
		}
	}

	std::string Cook(const CookRequest& request)
	{
		const fs::path outRoot = request.OutputRoot;

		KOI_LOG_INFO("Cook: output = {}", outRoot.string());

		std::error_code ec;
		fs::create_directories(outRoot, ec);

		// Cook layout: <out>/Engine/... and <out>/Project/...
		const fs::path projRoot = outRoot / "Project";

		const ShaderTally shaders = CookShaders(outRoot);

		for (const auto& [name, virtualPath] : request.Scenes)
		{
			if (!VirtualFileSystem::Get().TryResolveToDisk(virtualPath))
			{
				KOI_LOG_WARN("Cook: scene '{}' -> '{}' does not exist", name, virtualPath);
			}
		}

		// Content is shipped wholesale; a scene references its assets by the same relative paths
		CopyDir(request.EngineSource / "Content" / "Meshes",    outRoot / "Engine" / "Content" / "Meshes");
		CopyDir(request.EngineSource / "Content" / "Materials", outRoot / "Engine" / "Content" / "Materials");
		CopyDir(request.ProjectSource / "Content", projRoot / "Content");

		// Copy the project TOML as <out>/Project/Project.koi.toml
		if (fs::exists(request.ProjectFile))
		{
			fs::create_directories(projRoot, ec);
			std::error_code cec;
			fs::copy_file(request.ProjectFile, projRoot / "Project.koi.toml", fs::copy_options::overwrite_existing, cec);
			if (cec)
				KOI_LOG_WARN("Cook: copy project TOML failed: {}", cec.message());
		}

		std::string status = "Cook: " + std::to_string(shaders.Ok) + "/" + std::to_string(shaders.Total) + " shaders ok";
		KOI_LOG_INFO("{}", status);
		return status;
	}
}
