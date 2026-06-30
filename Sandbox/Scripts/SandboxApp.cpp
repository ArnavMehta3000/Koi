module;
#include "Core/Config.h"
#include <toml++/toml.hpp>
#include <cassert>
#include <cmath>
#include <algorithm>
module Koi.Sandbox.App;

import Koi.Core.Math;
import Koi.Core.Logger;
import Koi.Runtime.AppBase;
import Koi.Runtime.Engine;
import Koi.Runtime.CommandLine;
import Koi.Graphics.Renderer;
import Koi.Graphics.RenderWindow;
import Koi.Graphics.RHI.Device;
import Koi.Graphics.RendererResources;
import Koi.Graphics.RenderPasses.ScenePass;
import Koi.Graphics.RenderPasses.TonemapPass;
import Koi.Platform.InputManager;
import Koi.Platform.InputCodes;
import Koi.Memory.MemoryTracker;
import Koi.Scene;
#include "Core/Profiling.h"
#include "Core/Logger.h"
#include "Memory/MemoryTracker.h"

namespace koi
{
	std::unique_ptr<AppBase> CreateApplication(CommandLine& cmd)
	{
		auto project = toml::parse_file(cmd.GetProjectFile().string());

		AppCreateDesc desc{};
		desc.AppName           = project["project"]["name"].value_or("Koi Application");
		desc.WindowDesc.Title  = project["window"]["title"].value_or("Koi");
		desc.WindowDesc.Width  = project["window"]["width"].value_or(1280u);
		desc.WindowDesc.Height = project["window"]["height"].value_or(720u);
		desc.WindowDesc.VSync  = project["window"]["vsync"].value_or(true);
		desc.StartupScene      = project["assets"]["startup_scene"].value_or("");

		return std::make_unique<SandboxApp>(std::move(desc));
	}
}

void SandboxApp::OnInit()
{
	ZoneScoped;
	KOI_LOG_INFO("SandboxApp::OnInit()");
	KOI_MEM_TAG_SCOPE(koi::MemTag::User);
	LoadScene();
	SetupCamera();

#if KOI_WITH_EDITOR
	koi::Engine& engine = *GetEngine();

	std::ignore = engine.RegisterHook(koi::EnginePhase::OnImGuiDraw, [this]
	{
		m_scene.DrawImGui();
	});
#endif
}

void SandboxApp::OnTick(koi::f32 dt)
{
	ZoneScoped;
	koi::Engine& engine      = *GetEngine();
	koi::InputManager& input = engine.GetInputManager();

	m_scene.Update();

	if (const koi::RenderWindow* rw = engine.GetRenderer().GetMainRenderWindow())
		m_camera.SetAspect(static_cast<koi::f32>(rw->GetWidth()) / static_cast<koi::f32>(rw->GetHeight()));

	if (input.IsKeyPressed(koi::KeyCode::Escape))
		engine.RequestExit();

	if (m_controller)
		m_controller->Update(dt, input);
}

void SandboxApp::OnRender()
{
	ZoneScoped;
	koi::Engine& engine     = *GetEngine();
	koi::Renderer& renderer = engine.GetRenderer();

	koi::RenderWindow* rw = renderer.GetMainRenderWindow();

	renderer.GetFrameGraph().AddPass<koi::ScenePass>(koi::ScenePass::Desc
	{
		.Scene        = &m_scene,
		.Camera       = &m_camera,
		.Resources    = &renderer.GetDefaultResources(),
		.RenderWindow = rw,
		.DepthBuffer  = renderer.GetDepthBuffer(),
	});

	renderer.GetFrameGraph().AddPass<koi::TonemapPass>(koi::TonemapPass::Desc
	{
		.Device       = &renderer.GetDevice(),
		.Pipeline     = renderer.GetDefaultResources().TonemapPipeline,
		.HdrInput     = rw ? rw->GetHDRColorBuffer() : koi::TextureHandle{},
		.TargetWindow = rw,
	});
}

void SandboxApp::OnShutdown()
{
	ZoneScoped;
	KOI_LOG_INFO("SandboxApp::OnShutdown()");
	koi::GfxDevice& device = GetEngine()->GetRenderer().GetDevice();
	m_scene.Release(device);
}

void SandboxApp::LoadScene()
{
	ZoneScoped;
	KOI_MEM_TAG_SCOPE(koi::MemTag::User);
	KOI_LOG_INFO("SandboxApp::LoadScene()");

	const std::string& scenePath = GetDesc().StartupScene;
	assert(!scenePath.empty() && "No startup_scene set in project TOML");

	koi::Renderer& renderer = GetEngine()->GetRenderer();
	koi::GfxDevice& device  = renderer.GetDevice();
	const koi::DescriptorIndex whiteSRV =
		device.Get(renderer.GetDefaultResources().WhiteTexture)->SRVIndex;

	if (auto result = koi::Scene::Load(scenePath, device, whiteSRV))
		m_scene = std::move(result.value());
}

void SandboxApp::SetupCamera()
{
	ZoneScoped;
	KOI_MEM_TAG_SCOPE(koi::MemTag::User);

	const koi::math::AABB& bounds = m_scene.GetBounds();
	koi::f32 mn[3]{}, mx[3]{};
	koi::math::store(mn, bounds.Min);
	koi::math::store(mx, bounds.Max);

	const koi::math::float3 center{ 0.5f * (mn[0] + mx[0]), 0.5f * (mn[1] + mx[1]), 0.5f * (mn[2] + mx[2]) };
	const koi::f32 ex = 0.5f * (mx[0] - mn[0]);
	const koi::f32 ey = 0.5f * (mx[1] - mn[1]);
	const koi::f32 ez = 0.5f * (mx[2] - mn[2]);
	const koi::f32 radius = std::max(std::sqrt(ex * ex + ey * ey + ez * ez), 0.001f);

	m_camera.SetProjection(0.8f, GetWindow().AspectRatio(), 0.05f, 1000.0f);
	m_controller = std::make_unique<koi::CameraController>(&m_camera, koi::CameraMode::Orbit);
	m_controller->SetOrbitTarget(center, radius * 2.5f, 0.0f, 0.3f);
}
