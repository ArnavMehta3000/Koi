#include "Koi/Editor/Core/EditorContext.h"
#include "Koi/Editor/Core/EditorExtension.h"

namespace
{
	// Only editor builds compile this folder: register inspectors and panels here
	class {{Name}}Editor final : public koi::editor::EditorExtension
	{
	public:
		void OnStartup([[maybe_unused]] koi::editor::IEditorContext& ctx) override
		{
		}
	};
}

KOI_EDITOR_EXTENSION({{Name}}Editor);
