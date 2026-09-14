#pragma once
#include "Koi/Engine/Assets/MaterialAsset.h"

namespace koi::editor
{
	class AssetDatabase;
	struct AssetEntry;

	// Edits a .mat.toml in place. Which fields appear, and in what order, comes from the
	// material's template rather than from this file
	class MaterialInspector
	{
	public:
		void Draw(AssetDatabase& database, const AssetEntry& entry);

	private:
		void DrawTextureSlot(AssetDatabase& database, const MaterialParam& param);
		void Load(const AssetEntry& entry);
		void Save(AssetDatabase& database, const AssetEntry& entry);

		// Pushes the edit into the live material's table row, so the viewport shows it
		void ApplyToResident(const AssetEntry& entry) const;

		std::string    m_loaded;  // virtual path
		MaterialSource m_source;
		bool           m_dirty = false;
	};
}
