#include "Components/Obstacle.h"
#include "Koi/Engine/ECS/EntityBuilder.h"
#include "Koi/Engine/Interop/InspectorWidgets.h"
#include "Koi/Engine/Math/Scalar.h"
#include <algorithm>
#include <imgui.h>

namespace game
{
	void ObstacleSpawnerInspector::Draw(koi::Entity e)
	{
		ObstacleSpawner* s = e.try_get_mut<ObstacleSpawner>();
		if (!s) return;

		DrawLanes(s->Positions, -1);
		ImGui::InputScalar("Seed", ImGuiDataType_U64, &s->Seed);
		ImGui::DragFloat("Interval", &s->Interval, 0.01f, 0.05f, 10.0f, "%.2f s");
		ImGui::DragFloat("Min Interval", &s->MinInterval, 0.01f, 0.05f, 10.0f, "%.2f s");
		ImGui::DragFloat("Speed", &s->Speed, 0.1f, 0.1f, 100.0f, "%.1f u/s");
		ImGui::DragFloat("Max Speed", &s->MaxSpeed, 0.1f, 0.1f, 100.0f, "%.1f u/s");
		ImGui::DragFloat("Ramp Duration", &s->RampDuration, 0.5f, 1.0f, 600.0f, "%.0f s");
		koi::DrawEnumCombo("Ramp Ease", s->RampEase, koi::kEaseTypeNames);
		ImGui::DragFloat("Despawn Z", &s->DespawnZ, 0.1f);
		ImGui::DragFloat("Pop Duration", &s->PopDuration, 0.01f, 0.0f, 2.0f, "%.2f s");
		ImGui::Text("Elapsed %.1f s", s->Elapsed);
	}

	void ObstacleSpawnSystem::Each(const koi::SystemContext& ctx, koi::Entity, ObstacleSpawner& spawner)
	{
		spawner.Elapsed   += ctx.DeltaTime;
		spawner.Countdown -= ctx.DeltaTime;
		if (spawner.Countdown > 0.0f) return;

		const koi::f32 ramp     = koi::Ease(spawner.RampEase, spawner.Elapsed / std::max(spawner.RampDuration, 1e-3f));
		const koi::f32 interval = koi::Lerp(spawner.Interval, spawner.MinInterval, ramp);
		const koi::f32 speed    = koi::Lerp(spawner.Speed, spawner.MaxSpeed, ramp);
		spawner.Countdown = interval;

		const koi::i32 lane = spawner.Rng.Range(0, static_cast<koi::i32>(spawner.Positions.size()) - 1);

		koi::Spawn(ctx.World, "Obstacle")
			.At(spawner.Positions[static_cast<size_t>(lane)])
			.Scaled(0.0f)
			.WithMesh(spawner.Mesh)
			.WithMaterial(spawner.Material)
			.With(Obstacle{ .Speed = speed, .DespawnZ = spawner.DespawnZ, .PopDuration = spawner.PopDuration });
	}

	void ObstacleMoveSystem::Each(const koi::SystemContext& ctx, koi::Entity e, koi::Transform& transform, Obstacle& obstacle)
	{
		obstacle.Age += ctx.DeltaTime;
		transform.Position.z += obstacle.Speed * ctx.DeltaTime;
		transform.Scale = koi::Vector3::One * koi::Ease(koi::EaseType::OutBack, obstacle.Age / std::max(obstacle.PopDuration, 1e-3f));

		if (transform.Position.z > obstacle.DespawnZ)
		{
			e.destruct();
		}
	}
}
