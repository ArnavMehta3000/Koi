#include "Koi/Editor/Cooker.h"
#include "Koi/Editor/Assets/AssetDatabase.h"
#include "Koi/Engine/Graphics/PipelineLibrary.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Graphics/ShaderCache.h"
#include "Koi/Engine/Graphics/ShaderCompiler.h"
#include "Koi/Engine/IO/VirtualFileSystem.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Memory/MemoryTracker.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <system_error>
#include <unordered_set>
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

		static bool CopyRelative(const fs::path& projectSource, const fs::path& projRoot, const std::string& relative)
		{
			const fs::path src = projectSource / relative;
			if (!fs::exists(src)) return false;

			const fs::path dst = projRoot / relative;

			std::error_code ec;
			fs::create_directories(dst.parent_path(), ec);
			fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);

			if (ec)
			{
				KOI_LOG_WARN("Cook: copy {} failed: {}", relative, ec.message());
				return false;
			}
			return true;
		}

		static u32 CopyReachableAssets(const AssetDatabase& database, const fs::path& projectSource, const fs::path& projRoot)
		{
			std::vector<UUID> pending;
			std::unordered_set<UUID> visited;

			for (const AssetEntry& entry : database.All())
			{
				if (entry.Kind == AssetKind::Scene && !entry.Id.is_nil() && visited.insert(entry.Id).second)
					pending.push_back(entry.Id);
			}

			u32 copied = 0;
			while (!pending.empty())
			{
				const UUID id = pending.back();
				pending.pop_back();

				const AssetEntry* entry = database.Find(id);
				if (!entry) continue;

				if (entry->State == AssetState::Missing)
				{
					KOI_LOG_WARN("Cook: {} is referenced but missing on disk", entry->VirtualPath);
					continue;
				}

				constexpr std::string_view kProjectScheme = "project://";
				if (!entry->VirtualPath.starts_with(kProjectScheme)) continue;

				if (CopyRelative(projectSource, projRoot, entry->VirtualPath.substr(kProjectScheme.size())))
					++copied;

				for (const AssetKey& dep : database.DependenciesOf(id))
				{
					const UUID target = database.ResolveDependency(dep);
					if (target.is_nil())
					{
						if (dep.IsUri() && CopyRelative(projectSource, projRoot, std::string(dep.Uri())))
							++copied;
					}
					else if (visited.insert(target).second)
					{
						pending.push_back(target);
					}
				}
			}

			return copied;
		}
	}

	std::string Cook(const CookRequest& request, const AssetDatabase* database)
	{
		KOI_MEM_TAG_SCOPE(MemTag::Editor);
		const fs::path outRoot = request.OutputRoot;

		KOI_LOG_INFO("Cook: output = {}", outRoot.string());

		std::error_code ec;
		fs::create_directories(outRoot, ec);

		// Cook layout: <out>/Engine/... and <out>/Project/...
		const fs::path projRoot = outRoot / "Project";

		const ShaderTally shaders = CookShaders(outRoot);

		u32 assets = 0;
		if (database)
		{
			assets = CopyReachableAssets(*database, request.ProjectSource, projRoot);

			// Identity manifest: what lets a cooked build resolve the uuids a scene references
			fs::create_directories(projRoot, ec);
			database->SaveIdentityManifest(projRoot / "AssetRegistry.toml");
		}
		else
		{
			// No database means no edges to walk, so fall back to shipping the content wholesale
			CopyDir(request.ProjectSource / "Assets", projRoot / "Assets");
		}

		// Copy the project TOML as <out>/Project/Project.koi.toml
		if (fs::exists(request.ProjectFile))
		{
			fs::create_directories(projRoot, ec);
			std::error_code cec;
			fs::copy_file(request.ProjectFile, projRoot / "Project.koi.toml", fs::copy_options::overwrite_existing, cec);
			if (cec)
				KOI_LOG_WARN("Cook: copy project TOML failed: {}", cec.message());
		}

		std::string status = "Cook: " + std::to_string(shaders.Ok) + "/" + std::to_string(shaders.Total) + " shaders ok, " + std::to_string(assets) + " files";
		KOI_LOG_INFO("{}", status);
		return status;
	}
}
