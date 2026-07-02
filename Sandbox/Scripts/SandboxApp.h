#pragma once
#include "Runtime/AppBase.h"
#include "Graphics/Renderer.h"
#include "Graphics/Camera.h"
#include "Graphics/CameraController.h"
#include "Scene/Scene.h"
#include <memory>

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
	void SetupCamera();

private:
	koi::Scene                             m_scene;
	koi::Camera                            m_camera;
	std::unique_ptr<koi::CameraController> m_controller;
};
