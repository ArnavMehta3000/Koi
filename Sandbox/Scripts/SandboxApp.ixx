module;
#include "Core/Config.h"
#include <memory>
export module Koi.Sandbox.App;

import Koi.Core.Types;
import Koi.Core.Math;
import Koi.Runtime.AppBase;
import Koi.Graphics.Camera;
import Koi.Graphics.CameraController;
import Koi.Scene;

export class SandboxApp : public koi::AppBase
{
public:
	explicit SandboxApp(koi::AppCreateDesc desc) : AppBase(std::move(desc)) {}

	void OnInit() override;
	void OnTick(koi::f32 dt) override;
	void OnRender() override;
	void OnShutdown() override;

private:
	void LoadScene();
	void SetupCamera();

private:
	koi::Scene                             m_scene;
	koi::Camera                            m_camera;
	std::unique_ptr<koi::CameraController> m_controller;
};
