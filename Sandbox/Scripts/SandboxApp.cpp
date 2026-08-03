#include "SandboxApp.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Runtime/Engine.h"
#include "Koi/Engine/Runtime/CommandLine.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Graphics/RenderPasses/GBufferPass.h"
#include "Koi/Engine/Graphics/RenderPasses/TonemapPass.h"
#include "Koi/Engine/Graphics/RenderPasses/LightingPass.h"
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
	KOI_LOG_DEBUG("SandboxApp::OnInit()");
	KOI_MEM_TAG_SCOPE(koi::MemTag::User);

	koi::Renderer& renderer = GetEngine()->GetSubsystem<koi::Renderer>();
	koi::RenderGraph& graph = renderer.GetFrameGraph();

	m_gbufferPass = &graph.AddPass<koi::GBufferPass>(koi::GBufferPass::Config
	{
		.Device    = &renderer.GetDevice(),
		.Resources = &renderer.GetDefaultResources(),
	});

	m_lightingPass = &graph.AddPass<koi::LightingPass>(koi::LightingPass::Config
	{
		.Device    = &renderer.GetDevice(),
		.Resources = &renderer.GetDefaultResources(),
	});

	m_tonemapPass = &graph.AddPass<koi::TonemapPass>(koi::TonemapPass::Config
	{
		.Device   = &renderer.GetDevice(),
		.Pipeline = renderer.GetDefaultResources().TonemapPipeline,
	});

	LoadScene();
}

void SandboxApp::OnTick(koi::f32 dt)
{
	ZoneScoped;
	koi::Engine& engine      = *GetEngine();
	koi::InputManager& input = engine.GetSubsystem<koi::InputManager>();

	m_world.Update(dt);

	if (input.IsKeyPressed(koi::KeyCode::Escape))
		engine.RequestExit();
}

void SandboxApp::OnRender()
{
	ZoneScoped;

	const std::optional<koi::RenderView> view = GetRenderView(m_world);
	if (!view) return;  // Nothing valid to render into yet this frame

	const koi::ViewportTarget& target = view->Target;

	m_gbufferPass->SetWorld(&m_world)
		.SetCamera(view->Camera)
		.SetTarget(target.Width, target.Height);

	m_lightingPass->SetWorld(&m_world)
		.SetCamera(view->Camera)
		.SetGBuffer(m_gbufferPass->GetAlbedo(), m_gbufferPass->GetNormals(), m_gbufferPass->GetMaterial(), m_gbufferPass->GetDepth())
		.SetTarget(target.HDRColorBuffer, target.Width, target.Height);

	m_tonemapPass->SetInput(m_lightingPass->GetColorOutput())
		.SetTarget(target.ResolvedColorBuffer, target.Width, target.Height);
}

void SandboxApp::OnShutdown()
{
	ZoneScoped;
	KOI_LOG_DEBUG("SandboxApp::OnShutdown()");

	// Hands back the asset references the scene's entities took when they loaded
	m_world.Clear(GetEngine()->GetSubsystem<koi::Renderer>().GetDevice());
}

void SandboxApp::LoadScene()
{
	ZoneScoped;
	KOI_MEM_TAG_SCOPE(koi::MemTag::User);
	KOI_LOG_DEBUG("SandboxApp::LoadScene()");

	const std::string& scenePath = GetDesc().StartupScene;
	assert(!scenePath.empty() && "No startup_scene set in project TOML");

	koi::Renderer& renderer = GetEngine()->GetSubsystem<koi::Renderer>();
	koi::GfxDevice& device  = renderer.GetDevice();

	if (std::optional<koi::World> result = koi::World::Load(scenePath, device))
	{
		m_world = std::move(*result);
	}
}
