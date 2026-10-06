#pragma once
#include "Koi/Editor/Assets/TextureImporter.h"
#include "Koi/Engine/Interop/EditorPanel.h"
#include <string>

namespace koi::editor
{
	class AssetDatabase;
	class ImportQueue;

	class ImportPanel : public IEditorPanel
	{
	public:
		ImportPanel(ImportQueue& queue, AssetDatabase& database) : m_queue(queue), m_database(database) {}

		[[nodiscard]] std::string_view GetName() const override { return "Import"; }
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
		ImportQueue&   m_queue;
		AssetDatabase& m_database;
		TextureKind    m_allKind      = TextureKind::Color;
		bool           m_focusPending = false;
	};
}
