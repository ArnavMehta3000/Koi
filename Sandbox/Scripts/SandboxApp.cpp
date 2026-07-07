#include "SandboxApp.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Runtime/Engine.h"
#include "Koi/Engine/Runtime/CommandLine.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Graphics/RenderPasses/ScenePass.h"
#include "Koi/Engine/Graphics/RenderPasses/TonemapPass.h"
#include <toml++/toml.hpp>

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

	koi::Renderer& renderer = GetEngine()->GetRenderer();
	koi::RenderGraph& graph = renderer.GetFrameGraph();

	m_scenePass = &graph.AddPass<koi::ScenePass>(koi::ScenePass::Config
	{
		.Resources = &renderer.GetDefaultResources(),
	});

	m_tonemapPass = &graph.AddPass<koi::TonemapPass>(koi::TonemapPass::Config
	{
		.Device   = &renderer.GetDevice(),
		.Pipeline = renderer.GetDefaultResources().TonemapPipeline,
	});

	LoadScene();
}

void SandboxApp::OnTick(koi::f32 /*dt*/)
{
	ZoneScoped;
	koi::Engine& engine      = *GetEngine();
	koi::InputManager& input = engine.GetInputManager();

	m_scene.Update();
	m_scene.GetCamera().SetAspect(GetWindow().AspectRatio());

	if (input.IsKeyPressed(koi::KeyCode::Escape))
		engine.RequestExit();
}

void SandboxApp::OnRender()
{
	ZoneScoped;

	const std::optional<koi::ViewportTarget> target = GetFrameTarget();
	if (!target) return;  // Nothing valid to render into yet this frame

	const koi::Camera* camera = ResolveCamera(&m_scene.GetCamera());

	m_scenePass->SetScene(&m_scene)
		.SetCamera(camera)
		.SetTarget(target->HDRColorBuffer, target->DepthBuffer, target->Width, target->Height);

	m_tonemapPass->SetInput(m_scenePass->GetColorOutput())
		.SetTarget(target->ResolvedColorBuffer, target->Width, target->Height);
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

	if (std::optional<koi::Scene> result = koi::Scene::Load(scenePath, device, whiteSRV))
	{
		m_scene = std::move(*result);
	}

	m_scene.GetCamera().SetAspect(GetWindow().AspectRatio());
}
