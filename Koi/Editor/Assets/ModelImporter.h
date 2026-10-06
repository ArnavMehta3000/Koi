#pragma once
#include "Koi/Editor/Assets/ImportProgress.h"
#include "Koi/Engine/Assets/AssetFile.h"

namespace koi::editor
{
	// Stored verbatim in the .kmesh, so a reimport makes the same choices
	struct ModelImportSettings
	{
		f32 Scale           = 1.0f;
		u8  ImportMaterials = 1;  // and their textures; off leaves every slot on the fallback material
		u8  Reserved0[3]    = {};
		u32 Reserved[2]     = {};
	};
	static_assert(sizeof(ModelImportSettings) == 16 && std::is_trivially_copyable_v<ModelImportSettings>);

	inline constexpr u32 kModelImporterVersion = 1;

	[[nodiscard]] bool IsModelSource(const fs::path& path);

	struct ModelImportRequest
	{
		fs::path            Source;
		std::string         SourceRef;      // what the .kmesh remembers as its source
		fs::path            Folder;         // everything the model produces lands in here
		std::string         VirtualFolder;  // the same folder as the engine sees it
		std::string         Name;           // the .kmesh's name, no extension
		UUID                Id;
		ModelImportSettings Settings;
	};

	// Writes <Name>.kmesh plus a .kmat per material and a .ktex per texture it uses. Runs on an import worker
	[[nodiscard]] bool ImportModel(const ModelImportRequest& request, ImportProgress& progress, std::string& outError);
}
