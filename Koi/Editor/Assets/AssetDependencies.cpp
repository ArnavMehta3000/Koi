#include "Koi/Editor/Assets/AssetDependencies.h"
#include "Koi/Engine/Logging/Logger.h"
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <toml++/toml.hpp>
#include <algorithm>

namespace koi::editor
{
	namespace
	{
		constexpr std::string_view kProjectScheme = "project://";

		static void Add(std::vector<AssetKey>& out, AssetKey key)
		{
			if (key.IsValid() && std::ranges::find(out, key) == out.end())
			{
				out.push_back(std::move(key));
			}
		}

		// project-relative, forward slashes: the form the manifest keys its records by
		static AssetKey KeyForFile(const fs::path& file, const fs::path& projectSource)
		{
			std::error_code ec;
			const fs::path relative = fs::relative(file.lexically_normal(), projectSource, ec);
			if (ec || relative.empty() || *relative.begin() == "..")
			{
				return {};
			}

			return AssetKey::FromUri(relative.generic_string());
		}

		static void ScanTable(const toml::table& table, std::vector<AssetKey>& out);

		static void ScanNode(const toml::node& node, std::vector<AssetKey>& out)
		{
			if (const toml::table* table = node.as_table())
			{
				ScanTable(*table, out);
			}
			else if (const toml::array* array = node.as_array())
			{
				for (const toml::node& element : *array)  // Recurse
				{
					ScanNode(element, out);
				}
			}
		}

		static void ScanTable(const toml::table& table, std::vector<AssetKey>& out)
		{
			// UUID is more authoratative than path (which is for user or hinting)
			if (const std::optional<std::string> id = table["uuid"].value<std::string>())
			{
				Add(out, AssetKey(UUIDFromString(*id)));
			}
			else if (const std::optional<std::string> path = table["path"].value<std::string>(); path && path->starts_with(kProjectScheme))
			{
				Add(out, AssetKey::FromUri(path->substr(kProjectScheme.size())));
			}

			for (const auto& [key, value] : table) ScanNode(value, out);
		}

		static std::vector<AssetKey> ScanToml(const fs::path& diskPath, const fs::path&)
		{
			std::vector<AssetKey> deps;
			try
			{
				ScanTable(toml::parse_file(diskPath.string()), deps);
			}
			catch (const toml::parse_error& e)
			{
				KOI_LOG_WARN("Dependency scan: {} did not parse: {}", diskPath.string(), e.description());
			}
			return deps;
		}

		// Buffers and images that stayed as URIs
		static std::vector<AssetKey> ScanGltf(const fs::path& diskPath, const fs::path& projectSource)
		{
			auto data = fastgltf::GltfDataBuffer::FromPath(diskPath);
			if (data.error() != fastgltf::Error::None) return {};

			fastgltf::Parser parser;
			auto loadResult = parser.loadGltf(data.get(), diskPath.parent_path(), fastgltf::Options::None);
			if (!loadResult)
			{
				KOI_LOG_WARN("Dependency scan: {} did not load: {}", diskPath.string(), fastgltf::getErrorMessage(loadResult.error()).data());
				return {};
			}

			const fs::path base = diskPath.parent_path();
			std::vector<AssetKey> deps;

			const auto addUri = [&](const auto& source)
			{
				if (!source.uri.isLocalPath()) return;
				Add(deps, KeyForFile(base / source.uri.fspath(), projectSource));
			};

			for (const fastgltf::Buffer& buffer : loadResult->buffers)
			{
				if (const auto* uri = std::get_if<fastgltf::sources::URI>(&buffer.data))
					addUri(*uri);
			}

			for (const fastgltf::Image& image : loadResult->images)
			{
				if (const auto* uri = std::get_if<fastgltf::sources::URI>(&image.data))
					addUri(*uri);
			}

			return deps;
		}

		fs::path SidecarFor(const fs::path& diskPath)
		{
			fs::path sidecar = diskPath;
			sidecar.replace_extension();
			sidecar += kMeshSidecarExt;
			return sidecar;
		}

		static std::vector<AssetKey> ScanMesh(const fs::path& diskPath, const fs::path& projectSource)
		{
			std::vector<AssetKey> deps = ScanGltf(diskPath, projectSource);

			const fs::path sidecar = SidecarFor(diskPath);
			if (!fs::exists(sidecar)) return deps;

			Add(deps, KeyForFile(sidecar, projectSource));

			try
			{
				const toml::table root = toml::parse_file(sidecar.string());
				if (const toml::array* slots = root["slot"].as_array())
				{
					for (const toml::node& node : *slots)
					{
						if (const std::optional<std::string> key = node.as_table() ? (*node.as_table())["material"].value<std::string>() : std::nullopt)
						{
							Add(deps, AssetKey::Parse(*key));
						}
					}
				}
			}
			catch (const toml::parse_error& e)
			{
				KOI_LOG_WARN("Dependency scan: {} did not parse: {}", sidecar.string(), e.description());
			}

			return deps;
		}
	}

	std::vector<AssetKey> ScanDependencies(AssetKind kind, const fs::path& diskPath, const fs::path& projectSource)
	{
		if (diskPath.empty()) return {};

		switch (kind)
		{
		case AssetKind::Scene:
		case AssetKind::Material: return ScanToml(diskPath, projectSource);
		case AssetKind::Mesh:     return ScanMesh(diskPath, projectSource);
		default:                  return {};
		}
	}

	i64 DependencyStamp(AssetKind kind, const fs::path& diskPath)
	{
		if (kind != AssetKind::Mesh || diskPath.empty()) return 0;

		std::error_code ec;
		const fs::file_time_type time = fs::last_write_time(SidecarFor(diskPath), ec);
		return ec ? 0 : time.time_since_epoch().count();
	}
}
