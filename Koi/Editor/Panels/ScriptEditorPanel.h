#pragma once
#include "Koi/Engine/Interop/EditorPanel.h"
#include "Koi/Engine/Interop/Reloadable.h"
#include <ImGuiTextEditor.h>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace koi::editor
{
	namespace fs = std::filesystem;

	class ScriptEditorPanel : public IEditorPanel
	{
	public:
		explicit ScriptEditorPanel(std::function<void(const ReloadRequest&)> reload);

		[[nodiscard]] std::string_view GetName() const override { return "Text Editor"; }
		void OnDraw() override;

		void Open(std::string_view vfsPath);

	private:
		struct Document
		{
			std::string ScriptPath;  // VFS path; also the tab identity
			fs::path    DiskPath;
			TextEditor  Editor;
			size_t      SavedUndoIndex = 0;  // GetUndoIndex() at last load/save; mismatch = dirty
		};

		[[nodiscard]] bool IsDirty(const Document& doc) const;
		void Save(Document& doc);

	private:
		std::function<void(const ReloadRequest&)> m_reload;
		std::vector<Document> m_documents;
		std::string           m_focusRequest;  // vfs path to select on the next tab bar draw
	};
}
