#pragma once
#include "Koi/Editor/Assets/TextureImporter.h"
#include "Koi/Editor/Core/Panel.h"
#include <string>
#include <IconsFontAwesome6.h>

namespace koi::editor
{
	class AssetDatabase;
	class EditorContext;
	class ImportQueue;

	class ImportPanel : public Panel
	{
	public:
		static constexpr PanelInfo kInfo{ .Name = "Import", .Icon = ICON_FA_FILE_IMPORT };

		explicit ImportPanel(EditorContext* ctx);

		void OnDraw() override;

	private:
		void DrawDestination(const char* id, std::string& folder);
		void DrawPending();
		void DrawTextureTable(size_t& removeAt);
		void DrawModelTable(size_t& removeAt);
		void DrawProgress();

	private:
		ImportQueue*   m_queue    = nullptr;
		AssetDatabase* m_database = nullptr;
		TextureKind    m_allKind  = TextureKind::Color;
	};
}
