#include "Components/Obstacle.h"
#include "Components/PlayerMover.h"
#include "SurfApp.h"
#include "Koi/Editor/Core/EditorContext.h"
#include "Koi/Editor/Core/EditorExtension.h"
#include "Koi/Editor/Core/EditorModules.h"
#include "Koi/Modules/Physics/Editor/PhysicsEditor.h"
#include "Koi/Editor/Inspectors/InspectorRegistry.h"
#include "Koi/Editor/Inspectors/ReflectInspector.h"

namespace game
{
	namespace
	{
		class PlayerMoverInspector final : public koi::editor::ComponentInspector<PlayerMover>
		{
		public:
			void Draw(koi::Entity e) override
			{
				PlayerMover* mover = e.TryGet<PlayerMover>();
				if (!mover) return;

				DrawLaneSpheres(mover->Positions, *mover->Lane);
				koi::editor::utils::DrawReflected(*mover);
			}
		};

		class ObstacleSpawnerInspector final : public koi::editor::ComponentInspector<ObstacleSpawner>
		{
		public:
			void Draw(koi::Entity e) override
			{
				ObstacleSpawner* spawner = e.TryGet<ObstacleSpawner>();
				if (!spawner) return;

				DrawLaneSpheres(spawner->Positions, -1);
				koi::editor::utils::DrawReflected(*spawner);
			}
		};

		class SurfEditor final : public koi::editor::EditorExtension
		{
		public:
			void OnStartup(koi::editor::EditorContext& ctx) override
			{
				koi::editor::EditorModules<SurfApp::Modules>::Register(ctx);

				koi::editor::InspectorRegistry* inspectors = ctx.Get<koi::editor::InspectorRegistry>();
				inspectors->Emplace<PlayerMoverInspector>();
				inspectors->Emplace<ObstacleSpawnerInspector>();
				inspectors->Emplace<koi::editor::ReflectedInspector<Obstacle>>();
			}
		};
	}
}

KOI_EDITOR_EXTENSION(game::SurfEditor);
