#include "Koi/Editor/Core/EditorContext.h"
#include "Koi/Editor/Core/EditorExtension.h"

namespace
{
	// Only editor builds compile this folder. Reach editor services through ctx.Get<T>(), e.g. InspectorRegistry, PanelHost
	class {{Name}}Editor final : public koi::editor::EditorExtension
	{
	public:
		void OnStartup([[maybe_unused]] koi::editor::EditorContext& ctx) override
		{
		}
	};
}

KOI_EDITOR_EXTENSION({{Name}}Editor);
