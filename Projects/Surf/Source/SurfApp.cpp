#include "SurfApp.h"
#include "Koi/Engine/ECS/ComponentRegistry.h"
#include "Koi/Engine/ECS/SystemRegistry.h"
#include "Koi/Engine/Graphics/DebugDraw.h"
#include "Koi/Engine/Interop/EditorContext.h"
#include "Koi/Engine/Platform/InputManager.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <algorithm>
#include <imgui.h>

namespace koi
{
	std::unique_ptr<AppBase> CreateApplication([[maybe_unused]] CommandLine& cmd)
	{
		return std::make_unique<game::SurfApp>();
	}
}

namespace game
{
	void SurfApp::OnRegisterComponents()
	{
		koi::ComponentRegistry::Get().Register<koi::ComponentType<PlayerMover>>("game:player_mover", 50);
	}

	void SurfApp::OnRegisterSystems()
	{
		koi::SystemRegistry::Get().Register<PlayerMoverSystem>();
	}

	void SurfApp::OnInit()
	{
		if (koi::IEditorContext* editor = GetEditorContext())
			editor->RegisterInspector<PlayerMoverInspector>();
	}

	void SurfApp::OnTick([[maybe_unused]] koi::f32 dt)
	{
	}

	void PlayerMoverInspector::Draw(koi::Entity e)
	{
		PlayerMover* mover = e.try_get_mut<PlayerMover>();
		if (!mover) return;

		const char* labels[] = { "Left", "Center", "Right" };
		for (size_t i = 0; i < mover->Positions.size(); ++i)
		{
			ImGui::DragFloat3(labels[i], &mover->Positions[i].x, 0.05f, ImGuiSliderFlags_ColorMarkers);
		}
		ImGui::DragFloat("Speed", &mover->Speed, 0.1f, 0.1f, 100.0f, "%.1f u/s");
		ImGui::Text("Lane %d", mover->Lane);

		static const koi::Color activeCol(1.0f, 0.8f, 0.2f, 1.0f);
		static const koi::Color inactiveCol(0.3f, 0.9f, 1.0f, 1.0f);

		for (size_t i = 0; i < mover->Positions.size(); ++i)
		{
			const bool active = static_cast<koi::i32>(i) == mover->Lane;
			koi::DebugDraw::Sphere(mover->Positions[i], 0.25f, active ? activeCol : inactiveCol);
		}
	}

	void PlayerMoverSystem::Each(const koi::SystemContext& ctx, koi::Entity, koi::Transform& transform, PlayerMover& mover)
	{
		const koi::InputManager& input = koi::Engine::Get().GetSubsystem<koi::InputManager>();

		if (input.IsActionPressed("MoveLeft"))  --mover.Lane;
		if (input.IsActionPressed("MoveRight")) ++mover.Lane;
		mover.Lane = std::clamp(mover.Lane, 0, static_cast<koi::i32>(mover.Positions.size()) - 1);

		// Slide toward the lane at a constant speed, snapping once within one step
		const koi::Vector3 target = mover.Positions[mover.Lane];
		const koi::Vector3 delta  = target - transform.Position;
		const koi::f32     step   = mover.Speed * ctx.DeltaTime;

		transform.Position = delta.Length() <= step ? target : transform.Position + delta * (step / delta.Length());
	}
}
