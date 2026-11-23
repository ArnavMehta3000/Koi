#include "Koi/Engine/Resources/PrefabAsset.h"
#include "Koi/Engine/ECS/PrefabRegistry.h"
#include "Koi/Engine/IO/VirtualFileSystem.h"
#include <toml++/toml.hpp>

namespace koi
{
	PrefabSource::PrefabSource()  = default;
	PrefabSource::~PrefabSource() = default;

	Result<std::unique_ptr<PrefabSource>> AssetTraits<PrefabTemplate>::Load(const std::string& vfsPath)
	{
		const Result<std::vector<byte>> bytes = VirtualFileSystem::Get().Read(vfsPath);
		if (!bytes)
		{
			return std::unexpected(bytes.error());
		}

		auto source = std::make_unique<PrefabSource>();
		source->Path = vfsPath;
		try
		{
			const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
			source->File = std::make_unique<toml::table>(toml::parse(text, vfsPath));
		}
		catch (const toml::parse_error& e)
		{
			return Error::Make("line {}: {}", e.source().begin.line, e.description());
		}

		const std::string className = (*source->File)["class"].value_or(std::string());
		if (!PrefabRegistry::Get().Contains(className))
		{
			return Error::Make("its class '{}' isn't a registered prefab", className);
		}
		return source;
	}

	PrefabTemplate AssetTraits<PrefabTemplate>::Upload(GfxDevice&, const Source& source)
	{
		PrefabTemplate out{ PrefabRegistry::Get().Create((*source.File)["class"].value_or(std::string())) };
		out.Data->Load(*source.File, source.Path);
		return out;
	}

	void AssetTraits<PrefabTemplate>::Destroy(GfxDevice&, PrefabTemplate& resource)
	{
		resource.Data.reset();
	}
}
