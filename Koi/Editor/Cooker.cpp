#include "Koi/Editor/Cooker.h"
#include "Koi/Engine/Assets/AssetFile.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/RHI/PipelineCache.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/RHI/ShaderCache.h"
#include "Koi/Engine/IO/VirtualFileSystem.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <system_error>
#include <vector>

namespace koi::editor
{
	namespace
	{
		struct ShaderTally { u32 Ok = 0; u32 Total = 0; };

		// What imports are made from. The game only ever reads Koi's own formats
		constexpr std::string_view kSourceFiles[] =
		{
			".gltf", ".glb", ".bin", ".png", ".jpg", ".jpeg", ".tga", ".bmp", ".tif", ".tiff", ".gif", ".dds", ".hdr",
			".hlsl", ".hlsli", "shaders.json",
		};

		std::string Lower(std::string text)
		{
			std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return text;
		}

		// Binary assets lose their editor-only chunks on the way, source files are left behind
		static void CopyDir(const fs::path& src, const fs::path& dst, std::initializer_list<std::string_view> extraSkips = {})
		{
			ZoneScoped;
			std::error_code ec;
			if (!fs::is_directory(src, ec)) return;

			u32 copied = 0, cooked = 0;
			for (const fs::directory_entry& file : fs::recursive_directory_iterator(src, ec))
			{
				if (!file.is_regular_file()) continue;

				const std::string name = Lower(file.path().filename().string());
				const std::string ext  = Lower(file.path().extension().string());
				const auto matches = [&](std::string_view s) { return s == ext || s == name; };
				if (std::ranges::any_of(kSourceFiles, matches) || std::ranges::any_of(extraSkips, matches)) continue;

				const fs::path target = dst / fs::relative(file.path(), src, ec);
				fs::create_directories(target.parent_path(), ec);

				if (AssetFile::TypeForPath(file.path()) != AssetFileType::Unknown)
				{
					std::ifstream in(file.path(), std::ios::binary | std::ios::ate);
					std::vector<byte> bytes(static_cast<size_t>(in.tellg()));
					in.seekg(0);
					in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));

					const Result<AssetFile> asset = AssetFile::FromBytes(std::move(bytes));
					if (!asset)
					{
						KOI_LOG(Cook, Warn, "Cook: left out {}: {}", file.path().string(), asset.error().Message);
						continue;
					}

					const std::vector<byte> stripped = asset->Cooked();
					std::ofstream out(target, std::ios::binary | std::ios::trunc);
					if (!out.write(reinterpret_cast<const char*>(stripped.data()), static_cast<std::streamsize>(stripped.size())))
					{
						KOI_LOG(Cook, Warn, "Cook: couldn't write {}", target.string());
						continue;
					}
					++cooked;
					continue;
				}

				if (!fs::copy_file(file.path(), target, fs::copy_options::overwrite_existing, ec))
				{
					KOI_LOG(Cook, Warn, "Cook: couldn't copy {}: {}", file.path().string(), ec.message());
					continue;
				}
				++copied;
			}

			KOI_LOG(Cook, Debug, "Cook: {} asset(s) cooked and {} file(s) copied from {}", cooked, copied, src.string());
		}

		// xmake already compiled the shaders, the cook just checks they're current and ships them
		static ShaderTally CookShaders(const fs::path& outRoot)
		{
			ZoneScoped;

			ShaderCache& cache = ShaderCache::Get();
			ShaderTally tally;

			const auto& shaderPaths = Engine::Get().GetSubsystem<Renderer>().GetPipelineCache().CollectShaderPaths();
			for (const std::string& virtualPath : shaderPaths)
			{
				tally.Total++;
				switch (cache.GetBuildState(virtualPath))
				{
					case ShaderBuildState::Missing:
						KOI_LOG(Cook, Error, "Cook: {} isn't in any Shaders.json, so the cooked game won't have it", virtualPath);
						break;
					case ShaderBuildState::Stale:
						KOI_LOG(Cook, Warn, "Cook: {} changed since the last build. Run xmake before cooking or the game ships the old version", virtualPath);
						tally.Ok++;
						break;
					case ShaderBuildState::UpToDate:
						tally.Ok++;
						break;
				}
			}

			for (const auto [scheme, folder] : { std::pair{ "engine", "Engine" }, std::pair{ "project", "Project" } })
			{
				const auto manifest = VirtualFileSystem::Get().TryResolveToDisk(std::string(scheme) + "://Shaders/Manifest.toml");
				if (!manifest || !fs::exists(*manifest)) continue;

				const fs::path dst = outRoot / folder / "Shaders";
				std::error_code ec;
				fs::remove_all(dst, ec);  // so variants dropped since the last cook don't linger
				CopyDir(manifest->parent_path(), dst, { ".pdb" });
			}

			return tally;
		}
	}

	std::string Cook(const CookRequest& request)
	{
		const fs::path outRoot = request.OutputRoot;

		KOI_LOG(Cook, Info, "Cook: output = {}", outRoot.string());

		std::error_code ec;
		fs::create_directories(outRoot, ec);

		// Cook layout: <out>/Engine/... and <out>/Project/...
		const fs::path projRoot = outRoot / "Project";

		const ShaderTally shaders = CookShaders(outRoot);

		for (const auto& [name, virtualPath] : request.Scenes)
		{
			if (!VirtualFileSystem::Get().TryResolveToDisk(virtualPath))
			{
				KOI_LOG(Cook, Warn, "Cook: scene '{}' -> '{}' does not exist", name, virtualPath);
			}
		}

		// Content keeps its relative paths. The old copy goes first so nothing removed since lingers
		fs::remove_all(outRoot / "Engine" / "Content", ec);
		fs::remove_all(projRoot / "Content", ec);
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
			{
				KOI_LOG(Cook, Warn, "Cook: copy project TOML failed: {}", cec.message());
			}
		}

		std::string status = "Cook: " + std::to_string(shaders.Ok) + "/" + std::to_string(shaders.Total) + " shaders ok";
		KOI_LOG(Cook, Info, "{}", status);
		return status;
	}
}
