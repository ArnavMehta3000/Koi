#pragma  once
#include "Koi/Engine/Interop/EditorPanel.h"

namespace koi
{
	class GBufferDebugPanel : public IEditorPanel
	{
	public:
		[[nodiscard]] std::string_view GetName() const override { return "GBuffer Debug"; }


	private:
	};
}
