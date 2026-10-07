#include "Koi/Editor/Assets/MaterialWriter.h"
#include "Koi/Engine/Assets/AssetRegistry.h"
#include "Koi/Engine/Assets/MaterialFile.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Runtime/Engine.h"

namespace koi::editor
{
	bool WriteMaterialFile(const fs::path& file, const UUID& id, const MaterialSource& source)
	{
		const AssetIndex& index = Engine::Get().GetSubsystem<AssetRegistry>().Index();
		const MaterialFileContents contents = MaterialFileContents::FromSource(source, [&](const AssetKey& key)
		{
			if (const AssetIndexEntry* entry = index.FindByPath(key.Path()))
			{
				return entry->Id;
			}
			KOI_LOG(Import, Warn, "{} can't reference {} until it's imported as a .ktex, so that slot stays empty", file.filename().string(), key.Path());
			return UUID();
		});

		AssetFileWriter writer(AssetFileType::Material, id);
		contents.Write(writer);
		return writer.Save(file);
	}

	bool WriteMaterial(const fs::path& file, const MaterialSource& source)
	{
		if (AssetFile::TypeForPath(file) != AssetFileType::Material)
		{
			KOI_LOG(Import, Error, "Materials are saved as .kmat, so {} can't be written", file.filename().string());
			return false;
		}

		// Saving over a .kmat keeps its id, so meshes pointing at it don't lose it
		const std::optional<AssetFileHeader> existing = AssetFile::ReadHeader(file);
		return WriteMaterialFile(file, existing ? UUID(existing->Id) : utils::NewUUID(), source);
	}
}
