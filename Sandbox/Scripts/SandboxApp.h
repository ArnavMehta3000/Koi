#pragma once
#include "Koi/Engine/Runtime/AppBase.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/ECS/World.h"

namespace koi
{
	class ScenePass;
	class TonemapPass;
}

class SandboxApp : public koi::AppBase
{
public:
	explicit SandboxApp(koi::AppCreateDesc desc) : AppBase(std::move(desc)) {}

	void OnInit() override;
	void OnTick(koi::f32 dt) override;
	void OnRender() override;
	void OnShutdown() override;

	koi::World* GetEditorWorld() override { return &m_world; }

private:
	void LoadScene();

private:
	koi::World m_world;

	koi::ScenePass*   m_scenePass   = nullptr;
	koi::TonemapPass* m_tonemapPass = nullptr;
};
