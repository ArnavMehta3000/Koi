#include "Components/Obstacle.h"
#include "Components/PlayerMover.h"
#include "Koi/Editor/Core/EditorContext.h"
#include "Koi/Editor/Core/EditorExtension.h"
#include "Koi/Editor/Inspectors/ReflectInspector.h"

namespace game
{
	namespace
	{
		class PlayerMoverInspector final : public koi::editor::IComponentInspector
		{
		public:
			[[nodiscard]] std::string_view Key() const override { return PlayerMover::kKey; }

			void Draw(koi::Entity e) override
			{
				PlayerMover* mover = e.try_get_mut<PlayerMover>();
				if (!mover) return;

				DrawLaneSpheres(mover->Positions, *mover->Lane);
				koi::editor::utils::DrawReflected(*mover);
			}
		};

		class ObstacleSpawnerInspector final : public koi::editor::IComponentInspector
		{
		public:
			[[nodiscard]] std::string_view Key() const override { return ObstacleSpawner::kKey; }

			void Draw(koi::Entity e) override
			{
				ObstacleSpawner* spawner = e.try_get_mut<ObstacleSpawner>();
				if (!spawner) return;

				DrawLaneSpheres(spawner->Positions, -1);
				koi::editor::utils::DrawReflected(*spawner);
			}
		};

		class SurfEditor final : public koi::editor::EditorExtension
		{
		public:
			void OnStartup(koi::editor::IEditorContext& ctx) override
			{
				ctx.RegisterInspector<PlayerMoverInspector>();
				ctx.RegisterInspector<ObstacleSpawnerInspector>();
				ctx.RegisterInspector<koi::editor::ReflectedInspector<Obstacle>>();
			}
		};
	}
}

KOI_EDITOR_EXTENSION(game::SurfEditor);
