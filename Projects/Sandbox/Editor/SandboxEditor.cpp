#include "Components/PlayerController.h"
#include "Components/Spin.h"
#include "SandboxApp.h"
#include "Koi/Editor/Core/EditorContext.h"
#include "Koi/Editor/Core/EditorExtension.h"
#include "Koi/Editor/Core/EditorModules.h"
#include "Koi/Modules/Physics/Editor/PhysicsEditor.h"
#include "Koi/Editor/Inspectors/InspectorRegistry.h"
#include "Koi/Editor/Inspectors/ReflectInspector.h"
#include "Koi/Editor/UI/Widgets.h"
#include <imgui.h>

namespace
{
	// Speed gets a custom range and unit; a component without such needs can use ReflectedInspector<T> instead
	class SpinInspector final : public koi::editor::ComponentInspector<Spin>
	{
	public:
		void Draw(koi::Entity e) override
		{
			Spin* spin = e.TryGet<Spin>();
			if (!spin) return;

			if (!koi::editor::utils::BeginPropertyGrid("##spin")) return;

			koi::editor::utils::PropertyLabel("Axis");
			koi::editor::utils::DragAxes("##Axis", &spin->Axis.x, 3, 0.01f);
			koi::editor::utils::PropertyLabel("Speed");
			ImGui::DragFloat("##Speed", &spin->Speed, 1.0f, -1440.0f, 1440.0f, "%.1f deg/s");

			koi::editor::utils::EndPropertyGrid();
		}
	};

	class SandboxEditor final : public koi::editor::EditorExtension
	{
	public:
		void OnStartup(koi::editor::EditorContext& ctx) override
		{
			koi::editor::EditorModules<SandboxApp::Modules>::Register(ctx);

			koi::editor::InspectorRegistry* inspectors = ctx.Get<koi::editor::InspectorRegistry>();
			inspectors->Emplace<SpinInspector>();
			inspectors->Emplace<koi::editor::ReflectedInspector<PlayerController>>();
		}
	};
}

KOI_EDITOR_EXTENSION(SandboxEditor);
