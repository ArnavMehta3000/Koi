#include "Koi/Editor/Panels/AssetsPanel.h"
#include "Koi/Editor/Assets/AssetDatabase.h"
#include "Koi/Editor/Assets/ImportQueue.h"
#include "Koi/Engine/Graphics/ShaderCache.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <imgui.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <cctype>

namespace koi::editor
{
	namespace
	{
		// The folder tree shows the last path segment only; the full virtual path is the key
		std::string_view LeafName(std::string_view folder)
		{
			const size_t slash = folder.find_last_of('/');
			return slash == std::string_view::npos ? folder : folder.substr(slash + 1);
		}

		bool ContainsNoCase(std::string_view haystack, std::string_view needle)
		{
			const auto it = std::ranges::search(haystack, needle, [](char a, char b)
			{
				return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
			}).begin();

			return it != haystack.end();
		}

		// Residency has a different owner per kind: meshes and textures live in AssetCache,
		// shaders in ShaderCache. Scenes have no owner yet
		AssetState ResolveState(const AssetEntry& entry)
		{
			const auto resident = [](bool yes) -> AssetState { return yes ? AssetState::Loaded : AssetState::Unloaded; };

			Engine& engine = Engine::Get();
			switch (entry.Kind)
			{
			case AssetKind::Mesh:
				return resident(engine.GetSubsystem<AssetRegistry>().Cache<GPUMesh>().IsResident(entry.Key()));

			case AssetKind::Texture:
				return resident(engine.GetSubsystem<AssetRegistry>().Cache<GPUTexture>().IsResident(entry.Key()));

			case AssetKind::Shader:
			{
				// ShaderCache keys without the extension; the database entry carries it
				std::string_view path = entry.VirtualPath;
				if (const size_t dot = path.find_last_of('.'); dot != std::string_view::npos) path = path.substr(0, dot);
				return resident(ShaderCache::Get().IsCompiled(path));
			}

			default:
				return AssetState::Unloaded;
			}
		}

		ImVec4 StateColor(AssetState state)
		{
			switch (state)
			{
			case AssetState::Loaded:  return ImVec4(0.55f, 0.85f, 0.55f, 1.0f);
			default:                  return ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
			}
		}
	}

	AssetsPanel::AssetsPanel(AssetDatabase& database, Selection& selection, ImportQueue& imports)
		: m_database(database)
		, m_selection(selection)
		, m_imports(imports)
	{
		const std::span<const std::string> folders = m_database.Folders();
		if (!folders.empty()) m_selectedFolder = folders.front();
	}

	void AssetsPanel::SelectAsset(std::string virtualPath)
	{
		m_selection.SelectAsset(std::move(virtualPath));
	}

	void AssetsPanel::OnDraw()
	{
		if (ImGui::Begin("Assets", nullptr, ImGuiWindowFlags_MenuBar))
		{
			DrawToolbar();

			constexpr ImGuiTableFlags kSplitFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV;
			const f32 height = ImGui::GetContentRegionAvail().y;

			if (ImGui::BeginTable("Split", 2, kSplitFlags))
			{
				ImGui::TableSetupColumn("Folders", ImGuiTableColumnFlags_WidthFixed, 190.0f);
				ImGui::TableSetupColumn("Assets",  ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableNextRow();

				ImGui::TableNextColumn();
				ImGui::BeginChild("FolderTree", ImVec2(0, height));
				DrawFolderTree();
				ImGui::EndChild();

				ImGui::TableNextColumn();
				ImGui::BeginChild("AssetList", ImVec2(0, height));
				DrawAssetTable();
				ImGui::EndChild();

				ImGui::EndTable();
			}

			DrawDeleteModal();
			ApplyPendingAction();
		}
		ImGui::End();
	}

	void AssetsPanel::DrawToolbar()
	{
		if (!ImGui::BeginMenuBar()) return;

		if (ImGui::SmallButton("Refresh")) m_database.Refresh();

		ImGui::Separator();
		ImGui::SetNextItemWidth(220.0f);

		ImGui::InputTextWithHint("##Search", "Search all folders...", &m_search);

		ImGui::EndMenuBar();
	}

	void AssetsPanel::DrawFolderTree()
	{
		// Folders arrive sorted, so indenting by depth reproduces the hierarchy without
		// building a tree - the flat list is already in the right order
		for (const std::string& folder : m_database.Folders())
		{
			const u32 depth = static_cast<u32>(std::ranges::count(folder, '/')) - 2;  // "scheme://" contributes two

			ImGui::PushID(folder.c_str());
			ImGui::Indent(depth * 12.0f);

			const bool selected = (folder == m_selectedFolder);
			if (ImGui::Selectable(std::string(LeafName(folder)).c_str(), selected)) m_selectedFolder = folder;

			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", folder.c_str());

			// Dropping a row here moves the file. Only project folders can be written to
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kAssetDragDropType))
				{
					const std::string dropped(static_cast<const char*>(payload->Data));
					if (const AssetEntry* entry = m_database.Find(dropped); entry && entry->Writable)
					{
						m_pending     = Action::Move;
						m_pendingPath = entry->VirtualPath;
						m_pendingArg = folder;
					}
				}
				ImGui::EndDragDropTarget();
			}

			ImGui::Unindent(depth * 12.0f);
			ImGui::PopID();
		}
	}

	void AssetsPanel::DrawAssetTable()
	{
		constexpr ImGuiTableFlags kFlags = ImGuiTableFlags_Resizable
			| ImGuiTableFlags_RowBg
			| ImGuiTableFlags_ScrollY
			| ImGuiTableFlags_BordersInnerV;

		if (!ImGui::BeginTable("Assets", 3, kFlags)) return;

		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("Name",  ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Type",  ImGuiTableColumnFlags_WidthFixed, 70.0f);
		ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 70.0f);
		ImGui::TableHeadersRow();

		for (const AssetEntry& entry : m_database.All())
		{
			if (PassesFilter(entry)) DrawRow(entry);
		}

		ImGui::EndTable();

		if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered())
			if (m_selection.HasAsset()) m_selection.Clear();
	}

	void AssetsPanel::DrawRow(const AssetEntry& entry)
	{
		const AssetTypeInfo* type  = utils::FindAssetTypeByKind(entry.Kind);
		const AssetState     state = ResolveState(entry);

		ImGui::TableNextRow();
		ImGui::PushID(entry.VirtualPath.c_str());

		ImGui::TableNextColumn();
		DrawNameCell(entry);

		ImGui::TableNextColumn();
		if (type) ImGui::TextDisabled("%.*s", static_cast<int>(type->Name.size()), type->Name.data());

		ImGui::TableNextColumn();
		const std::string_view label = utils::ToString(state);
		ImGui::TextColored(StateColor(state), "%.*s", static_cast<int>(label.size()), label.data());

		ImGui::PopID();
	}

	void AssetsPanel::DrawNameCell(const AssetEntry& entry)
	{
		// A row being renamed swaps its label for the edit field, so the list never reflows
		if (entry.VirtualPath == m_renamingPath)
		{
			ImGui::SetNextItemWidth(-FLT_MIN);
			const bool committed = ImGui::InputText("##Rename", &m_renameBuffer,
				ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

			if (m_renameFocusPending)
			{
				ImGui::SetKeyboardFocusHere(-1);
				m_renameFocusPending = false;
			}

			if (committed && !m_renameBuffer.empty())
			{
				m_pending      = Action::Rename;
				m_pendingPath  = entry.VirtualPath;
				m_pendingArg   = m_renameBuffer;
				m_renamingPath.clear();
			}
			else if (committed || ImGui::IsItemDeactivated())
			{
				m_renamingPath.clear();  // click-away or Escape leaves the name alone
			}
			return;
		}

		const bool selected = (entry.VirtualPath == m_selection.GetAsset());

		if (ImGui::Selectable(entry.Name.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns))
			SelectAsset(entry.VirtualPath);

		// Payload is the virtual path; the receiving side resolves it back through the database
		if (ImGui::BeginDragDropSource())
		{
			ImGui::SetDragDropPayload(kAssetDragDropType, entry.VirtualPath.c_str(),
				entry.VirtualPath.size() + 1);
			ImGui::TextUnformatted(entry.Name.c_str());
			ImGui::EndDragDropSource();
		}

		DrawContextMenu(entry);
	}

	void AssetsPanel::DrawContextMenu(const AssetEntry& entry)
	{
		if (!ImGui::BeginPopupContextItem()) return;

		ImGui::TextDisabled("%s", entry.VirtualPath.c_str());
		ImGui::Separator();

		// Engine content is not the project's to edit
		const bool editable = entry.Writable;

		if (ImGui::MenuItem("Rename", nullptr, false, editable))
		{
			const std::string_view extension = utils::AssetExtensionOf(entry.Name);
			m_renamingPath       = entry.VirtualPath;
			m_renameBuffer       = entry.Name.substr(0, entry.Name.size() - extension.size());
			m_renameFocusPending = true;
		}

		// Opening the popup here would nest it under this menu's id; the flag defers that
		// to window scope, where the modal itself is drawn
		if (ImGui::MenuItem("Delete", nullptr, false, editable))
		{
			m_pendingPath     = entry.VirtualPath;
			m_pendingLabel    = entry.Name;
			m_openDeleteModal = true;
		}

		const AssetFileType fileType = AssetFile::TypeForPath(entry.DiskPath);

		// Runs in the background; the database only refreshes once it's done, so the entry stays valid
		const bool imported = fileType == AssetFileType::Texture || fileType == AssetFileType::Mesh;
		if (imported && ImGui::MenuItem("Reimport", nullptr, false, editable))
			m_imports.Reimport(entry.VirtualPath);

		ImGui::Separator();
		if (ImGui::MenuItem("Copy Virtual Path")) ImGui::SetClipboardText(entry.VirtualPath.c_str());

		ImGui::EndPopup();
	}

	void AssetsPanel::DrawDeleteModal()
	{
		if (std::exchange(m_openDeleteModal, false)) ImGui::OpenPopup("Delete Asset");

		ImGui::SetNextWindowSize(ImVec2(480.0f, 0.0f));
		if (!ImGui::BeginPopupModal("Delete Asset", nullptr, ImGuiWindowFlags_NoResize)) return;

		const std::vector<std::string> referencers = m_database.ReferencersOf(m_pendingPath);

		ImGui::TextWrapped("Delete '%s' from disk?", m_pendingLabel.c_str());
		ImGui::Spacing();

		if (referencers.empty())
		{
			ImGui::TextDisabled("Nothing else references it");
		}
		else
		{
			ImGui::TextColored(ImVec4(0.95f, 0.80f, 0.40f, 1.0f), "Referenced by %zu file(s):", referencers.size());

			if (ImGui::BeginChild("Referencers", ImVec2(0.0f, 96.0f), ImGuiChildFlags_Borders))
			{
				for (const std::string& path : referencers)
				{
					ImGui::BulletText("%s", path.c_str());
				}
			}
			ImGui::EndChild();

			ImGui::TextDisabled("Those references will dangle until they are edited");
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		if (ImGui::Button(referencers.empty() ? "Delete" : "Delete Anyway", ImVec2(140.0f, 0.0f)))
		{
			m_pending = Action::Delete;
			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f))) ImGui::CloseCurrentPopup();

		ImGui::EndPopup();
	}

	void AssetsPanel::ApplyPendingAction()
	{
		const Action action = std::exchange(m_pending, Action::None);
		if (action == Action::None) return;

		switch (action)
		{
		case Action::Rename: m_database.Rename(m_pendingPath, m_pendingArg); break;
		case Action::Move:   m_database.Move  (m_pendingPath, m_pendingArg); break;
		case Action::Delete: m_database.Delete(m_pendingPath);              break;
		default: break;
		}

		// Every path the panel was holding onto may have just changed underneath it
		if (m_selection.HasAsset()) m_selection.Clear();
		m_pendingArg.clear();
		m_pendingPath.clear();
	}

	bool AssetsPanel::PassesFilter(const AssetEntry& entry) const
	{
		if (!m_search.empty()) return ContainsNoCase(entry.Name, m_search);
		return entry.Folder == m_selectedFolder;
	}
}
