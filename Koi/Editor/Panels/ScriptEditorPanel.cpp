#include "Koi/Editor/Panels/ScriptEditorPanel.h"
#include "Koi/Engine/IO/VirtualFileSystem.h"
#include "Koi/Engine/Logging/Logger.h"
#include <imgui.h>
#include <algorithm>
#include <fstream>

namespace koi::editor
{
	ScriptEditorPanel::ScriptEditorPanel(std::function<void(const ReloadRequest&)> reload)
		: m_reload(std::move(reload))
	{
	}

	bool ScriptEditorPanel::IsDirty(const Document& doc) const
	{
		return doc.Editor.GetUndoIndex() != doc.SavedUndoIndex;
	}

	void ScriptEditorPanel::Open(std::string_view vfsPath)
	{
		const auto existing = std::ranges::find(m_documents, vfsPath, &Document::ScriptPath);
		if (existing != m_documents.end())
		{
			m_focusRequest = vfsPath;
			return;
		}

		const std::optional<fs::path> disk = VirtualFileSystem::Get().TryResolveToDisk(vfsPath);
		if (!disk)
		{
			KOI_LOG_ERROR("Script editor: cannot resolve '{}' to disk", vfsPath);
			return;
		}

		const std::optional<std::vector<byte>> bytes = VirtualFileSystem::Get().Read(vfsPath);
		if (!bytes)
		{
			KOI_LOG_ERROR("Script editor: failed to read '{}'", vfsPath);
			return;
		}

		Document& doc = m_documents.emplace_back();
		doc.ScriptPath = vfsPath;
		doc.DiskPath   = *disk;
		doc.Editor.SetLanguage(doc.DiskPath.extension() == ".hlsl" ? TextEditor::Language::Hlsl() : TextEditor::Language::AngelScript());
		doc.Editor.SetText(std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
		doc.SavedUndoIndex = doc.Editor.GetUndoIndex();

		m_focusRequest = vfsPath;
	}

	void ScriptEditorPanel::Save(Document& doc)
	{
		std::ofstream out(doc.DiskPath, std::ios::trunc);
		if (!out)
		{
			KOI_LOG_ERROR("Script editor: failed to write '{}'", doc.DiskPath.string());
			return;
		}

		out << doc.Editor.GetText();
		out.close();

		doc.SavedUndoIndex = doc.Editor.GetUndoIndex();

		KOI_LOG_INFO("Script editor: saved '{}'", doc.ScriptPath);
		m_reload(ReloadRequest{ .Scope = ReloadScope::Changed });
	}

	void ScriptEditorPanel::OnDraw()
	{
		if (ImGui::Begin("Text Editor"))
		{
			if (m_documents.empty())
			{
				ImGui::TextUnformatted("No script open");
			}
			else if (ImGui::BeginTabBar("##scripts", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs))
			{
				size_t closeIndex = m_documents.size();

				for (size_t i = 0; i < m_documents.size(); ++i)
				{
					Document& doc = m_documents[i];
					const bool dirty = IsDirty(doc);

					const ImGuiTabItemFlags flags = (doc.ScriptPath == m_focusRequest) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;

					// Unsaved-changes dot in the tab label, like most text editors
					const std::string label = dirty ? doc.ScriptPath + " *" : doc.ScriptPath;

					bool open = true;
					if (ImGui::BeginTabItem(label.c_str(), &open, flags))
					{
						const ImGuiIO& io = ImGui::GetIO();
						if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S))
							Save(doc);

						doc.Editor.Render(doc.ScriptPath.c_str());
						ImGui::EndTabItem();
					}

					if (!open) closeIndex = i;
				}

				if (closeIndex < m_documents.size())
					m_documents.erase(m_documents.begin() + static_cast<ptrdiff_t>(closeIndex));

				ImGui::EndTabBar();
			}

			m_focusRequest.clear();
		}
		ImGui::End();
	}
}
