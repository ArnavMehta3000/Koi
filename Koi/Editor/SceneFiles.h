#pragma once
#include "Koi/Engine/Core/Types.h"
#include <string>
#include <string_view>

namespace koi::editor
{
	class AssetDatabase;
	class EditorContext;
	class Selection;
	class Toasts;
	class UndoStack;

	class SceneFiles : public NonCopyable
	{
	public:
		explicit SceneFiles(EditorContext* ctx);

		void Open(std::string virtualPath);
		void Create(std::string_view name);   // Writes Content/Scenes/<name>.scene.toml
		void OnTick(f32 dt);

	private:
		EditorContext* m_ctx       = nullptr;
		AssetDatabase* m_database  = nullptr;
		Selection*     m_selection = nullptr;
		Toasts*        m_toasts    = nullptr;
		UndoStack*     m_undo      = nullptr;
		std::string    m_pending;
	};
}
