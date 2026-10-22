#pragma once
#include "Koi/Editor/Assets/TextureImporter.h"
#include "Koi/Editor/Core/EditorPanel.h"
#include <string>
#include <IconsFontAwesome6.h>

namespace koi::editor
{
	class AssetDatabase;
	class EditorContext;
	class ImportQueue;

	class ImportPanel : public IEditorPanel
	{
	public:
		explicit ImportPanel(EditorContext* ctx);

		[[nodiscard]] std::string_view GetName() const override { return "Import"; }
		[[nodiscard]] const char* GetIcon() const override { return ICON_FA_FILE_IMPORT; }
		void OnUpdate() override;
		void OnDraw() override;

		// Opens the panel and brings it to the front on the next frame
		void Focus() { SetOpen(true); m_focusPending = true; }

	private:
		void DrawDestination(const char* id, std::string& folder);
		void DrawPending();
		void DrawTextureTable(size_t& removeAt);
		void DrawModelTable(size_t& removeAt);
		void DrawProgress();

	private:
		ImportQueue*   m_queue    = nullptr;
		AssetDatabase* m_database = nullptr;
		TextureKind    m_allKind      = TextureKind::Color;
		bool           m_focusPending = false;
	};
}
