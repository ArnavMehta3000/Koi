#pragma once
#include "Koi/Engine/Interop/EditorPanel.h"
#include <functional>
#include <string>
#include <string_view>

namespace koi::editor
{
	class AssetDatabase;
	struct AssetEntry;

	// Folder tree on the left, contents of the selected folder on the right. Everything it
	// lists comes from AssetDatabase - the panel knows nothing about extensions or the disk.
	class AssetsPanel : public IEditorPanel
	{
	public:
		AssetsPanel(AssetDatabase& database, std::function<void(std::string_view)> openScript);

		[[nodiscard]] std::string_view GetName() const override { return "Assets"; }
		void OnDraw() override;

	private:
		void DrawToolbar();
		void DrawFolderTree();
		void DrawAssetTable();
		void DrawRow(const AssetEntry& entry);

		// Search spans every folder; with no search, only the selected folder is listed
		[[nodiscard]] bool PassesFilter(const AssetEntry& entry) const;

	private:
		AssetDatabase&                        m_database;
		std::function<void(std::string_view)> m_openScript;

		std::string m_selectedFolder;
		std::string m_selectedAsset;   // virtual path; empty when nothing is selected
		std::string m_search;
	};
}
