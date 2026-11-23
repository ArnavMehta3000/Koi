#include "Koi/Modules/Physics/Editor/PhysicsEditor.h"
#include "Koi/Modules/Physics/Source/PhysicsComponents.h"
#include "Koi/Editor/Core/EditorContext.h"
#include "Koi/Editor/Inspectors/InspectorRegistry.h"
#include "Koi/Editor/Inspectors/ReflectInspector.h"

namespace koi
{
	void PhysicsModule::Editor::Register(editor::EditorContext& ctx)
	{
		editor::InspectorRegistry* inspectors = ctx.Get<editor::InspectorRegistry>();
		inspectors->Emplace<editor::ReflectedInspector<PhysicsSettings>>();
		inspectors->Emplace<editor::ReflectedInspector<RigidBody>>();
		inspectors->Emplace<editor::ReflectedInspector<Collider>>();
	}
}
