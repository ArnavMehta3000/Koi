#pragma once
#include "Runtime/AppBase.h"
#include "Graphics/Renderer.h"
#include "Scene/Scene.h"

class SandboxApp : public koi::AppBase
{
public:
	explicit SandboxApp(koi::AppCreateDesc desc) : AppBase(std::move(desc)) {}

	void OnInit() override;
	void OnTick(koi::f32 dt) override;
	void OnRender() override;
	void OnShutdown() override;

	koi::Scene* GetEditorScene() override { return &m_scene; }

private:
	void LoadScene();

private:
	koi::Scene m_scene;
};
