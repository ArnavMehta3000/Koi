#pragma once
#include "Koi/Editor/Selection.h"
#include "Koi/Engine/Core/UUID.h"
#include "Koi/Engine/Interop/EditorPanel.h"
#include <functional>
#include <string>
#include <string_view>

namespace koi::editor
{
	class AssetDatabase;
	struct AssetEntry;

	// Queries the asset database to draw
	class AssetsPanel : public IEditorPanel
	{
	public:
		AssetsPanel(AssetDatabase& database, Selection& selection, std::function<void(std::string_view)> openScript);

		[[nodiscard]] std::string_view GetName() const override { return "Assets"; }
		void OnDraw() override;

	private:
		void DrawToolbar();
		void DrawFolderTree();
		void DrawAssetTable();
		void DrawRow(const AssetEntry& entry);
		void DrawNameCell(const AssetEntry& entry);
		void DrawContextMenu(const AssetEntry& entry);
		void DrawDeleteModal();
		void ApplyPendingAction();  // Called outside the draw loop to prevent span invalidation

		void SelectAsset(std::string virtualPath);

		// Search spans every folder, with no search, only the selected folder is listed
		[[nodiscard]] bool PassesFilter(const AssetEntry& entry) const;

	private:
		enum class Action { None, Rename, Move, Delete, Import };

	private:
		AssetDatabase&                        m_database;
		Selection&                            m_selection;
		std::function<void(std::string_view)> m_openScript;

		std::string m_selectedFolder;
		std::string m_selectedAsset;   // virtual path; empty when nothing is selected
		std::string m_search;

		UUID        m_renamingId{};
		std::string m_renameBuffer;
		bool        m_renameFocusPending = false;

		bool        m_openDeleteModal = false;
		Action      m_pending         = Action::None;
		UUID        m_pendingId{};
		std::string m_pendingArg;            // new base name for Rename, target folder for Move
		std::string m_pendingLabel;          // what the delete modal names
		bool        m_pendingForce = false;  // delete past the referencer check
	};
}
