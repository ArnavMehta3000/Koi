#include "SandboxApp.h"
#include "Core/Profiling.h"
#include "Runtime/Engine.h"
#include "Runtime/CommandLine.h"
#include "Graphics/RenderWindow.h"
#include "Graphics/RHI/Device.h"
#include "Graphics/RenderPasses/ScenePass.h"
#include "Graphics/RenderPasses/TonemapPass.h"
#include "Editor/EditorContext.h"
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
	koi::Engine& engine     = *GetEngine();
	koi::Renderer& renderer = engine.GetRenderer();
	koi::RenderGraph& graph = renderer.GetFrameGraph();

	koi::TextureHandle colorTarget, depthTarget, presentTarget;
	koi::u32 width = 0, height = 0;

	koi::IEditorContext* ctx = GetEditorContext();
	if (ctx)
	{
		const koi::ViewportTarget* vp = ctx->GetViewportTarget();
		if (!vp) return;  // Viewport panel not laid out with a valid size yet this frame

		colorTarget   = vp->HDRColorBuffer;
		depthTarget   = vp->DepthBuffer;
		presentTarget = vp->ResolvedColorBuffer;
		width  = vp->Width;
		height = vp->Height;
	}
	else
	{
		koi::RenderWindow* rw = renderer.GetMainRenderWindow();
		if (!rw) return;

		colorTarget   = rw->GetHDRColorBuffer();
		depthTarget   = renderer.GetDepthBuffer();
		presentTarget = rw->CurrentBackBuffer();
		width         = rw->GetWidth();
		height        = rw->GetHeight();
	}

	const koi::Camera* camera = ctx ? ctx->GetEditorCamera() : &m_scene.GetCamera();

	graph.AddPass<koi::ScenePass>(koi::ScenePass::Desc
	{
		.Scene       = &m_scene,
		.Camera      = camera,
		.Resources   = &renderer.GetDefaultResources(),
		.ColorTarget = colorTarget,
		.DepthBuffer = depthTarget,
		.Width       = width,
		.Height      = height,
	});

	graph.AddPass<koi::TonemapPass>(koi::TonemapPass::Desc
	{
		.Device      = &renderer.GetDevice(),
		.Pipeline    = renderer.GetDefaultResources().TonemapPipeline,
		.HdrInput    = colorTarget,
		.ColorTarget = presentTarget,
		.Width       = width,
		.Height      = height,
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

	if (std::optional<koi::Scene> result = koi::Scene::Load(scenePath, device, whiteSRV))
	{
		m_scene = std::move(*result);
	}

	m_scene.GetCamera().SetAspect(GetWindow().AspectRatio());
}
