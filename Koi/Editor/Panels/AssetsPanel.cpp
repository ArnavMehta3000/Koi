#include "Koi/Editor/Panels/AssetsPanel.h"
#include "Koi/Editor/Assets/AssetDatabase.h"
#include "Koi/Engine/Assets/AssetRegistry.h"
#include "Koi/Engine/Graphics/ShaderCache.h"
#include "Koi/Engine/Scripting/ScriptEngine.h"
#include "Koi/Engine/Memory/MemoryTracker.h"
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
		// shaders in ShaderCache, scripts in ScriptEngine. Scenes have no owner yet
		AssetState ResolveState(const AssetEntry& entry)
		{
			if (entry.State == AssetState::Missing) return AssetState::Missing;

			const auto resident = [](bool yes) { return yes ? AssetState::Loaded : AssetState::Unloaded; };

			Engine& engine = Engine::Get();
			switch (entry.Kind)
			{
			case AssetKind::Mesh:
				return resident(engine.GetSubsystem<AssetRegistry>().Cache<GPUMesh>().IsResident(entry.VirtualPath));

			case AssetKind::Texture:
				return resident(engine.GetSubsystem<AssetRegistry>().Cache<GPUTexture>().IsResident(entry.VirtualPath));

			case AssetKind::Shader:
			{
				// ShaderCache keys without the extension; the database entry carries it
				std::string_view path = entry.VirtualPath;
				if (const size_t dot = path.find_last_of('.'); dot != std::string_view::npos) path = path.substr(0, dot);
				return resident(ShaderCache::Get().IsCompiled(path));
			}

			case AssetKind::Script:
				return resident(engine.GetSubsystem<ScriptEngine>().IsModuleLoaded(entry.VirtualPath));

			default:
				return AssetState::Unloaded;
			}
		}

		ImVec4 StateColor(AssetState state)
		{
			switch (state)
			{
			case AssetState::Loaded:  return ImVec4(0.55f, 0.85f, 0.55f, 1.0f);
			case AssetState::Stale:   return ImVec4(0.95f, 0.80f, 0.40f, 1.0f);
			case AssetState::Missing:
			case AssetState::Error:   return ImVec4(0.95f, 0.45f, 0.45f, 1.0f);
			default:                  return ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
			}
		}
	}

	AssetsPanel::AssetsPanel(AssetDatabase& database, std::function<void(std::string_view)> openScript)
		: m_database(database)
		, m_openScript(std::move(openScript))
	{
		const std::span<const std::string> folders = m_database.Folders();
		if (!folders.empty()) m_selectedFolder = folders.front();
	}

	void AssetsPanel::OnDraw()
	{
		KOI_MEM_TAG_SCOPE(MemTag::Editor);

		if (ImGui::Begin("Assets", nullptr, ImGuiWindowFlags_MenuBar))
		{
			DrawToolbar();

			// A resizable table beats two sibling children here: the column divider is a real,
			// visible drag handle instead of a few pixels of child border
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

		if (const u32 missing = m_database.MissingCount(); missing > 0)
		{
			ImGui::SameLine();
			ImGui::TextColored(StateColor(AssetState::Missing), "%u missing", missing);
		}

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
			m_selectedAsset.clear();
	}

	void AssetsPanel::DrawRow(const AssetEntry& entry)
	{
		const AssetTypeInfo* type  = FindAssetTypeByKind(entry.Kind);
		const AssetState     state = ResolveState(entry);

		ImGui::TableNextRow();
		ImGui::PushID(entry.VirtualPath.c_str());

		ImGui::TableNextColumn();
		const bool selected = (entry.VirtualPath == m_selectedAsset);
		if (ImGui::Selectable(entry.Name.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns))
			m_selectedAsset = entry.VirtualPath;

		if (type && type->TextEditable && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			m_openScript(entry.VirtualPath);

		// Payload is the virtual path: the receiving side resolves it through the database,
		// so this keeps working unchanged once references become UUIDs
		if (ImGui::BeginDragDropSource())
		{
			ImGui::SetDragDropPayload(kAssetDragDropType, entry.VirtualPath.c_str(),
				entry.VirtualPath.size() + 1);
			ImGui::TextUnformatted(entry.Name.c_str());
			ImGui::EndDragDropSource();
		}

		if (ImGui::BeginPopupContextItem())
		{
			ImGui::TextDisabled("%s", entry.VirtualPath.c_str());
			ImGui::Separator();
			if (ImGui::MenuItem("Copy Virtual Path")) ImGui::SetClipboardText(entry.VirtualPath.c_str());
			if (!entry.Id.is_nil() && ImGui::MenuItem("Copy UUID"))
				ImGui::SetClipboardText(ToString(entry.Id).c_str());
			ImGui::EndPopup();
		}

		ImGui::TableNextColumn();
		if (type) ImGui::TextUnformatted(type->Name.data(), type->Name.data() + type->Name.size());

		ImGui::TableNextColumn();
		const std::string_view label = ToString(state);
		ImGui::TextColored(StateColor(state), "%.*s", static_cast<int>(label.size()), label.data());

		ImGui::PopID();
	}

	bool AssetsPanel::PassesFilter(const AssetEntry& entry) const
	{
		if (!m_search.empty()) return ContainsNoCase(entry.Name, m_search);
		return entry.Folder == m_selectedFolder;
	}
}
