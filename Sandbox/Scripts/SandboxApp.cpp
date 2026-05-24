#include "SandboxApp.h"
#include "Runtime/Engine.h"
#include "Runtime/CommandLine.h"
#include "Graphics/RenderWindow.h"
#include "Graphics/RHI/CommandList.h"
#include "Graphics/RHI/PipelineDesc.h"
#include "Graphics/ShaderLoader.h"
#include "Graphics/MeshLoader.h"
#include <toml++/toml.hpp>

namespace koi
{
	std::unique_ptr<AppBase> CreateApplication(CommandLine& cmd)
	{
		auto projectFile = toml::parse_file(cmd.GetProjectFile().string());

		AppCreateDesc desc{};
		desc.AppName           = projectFile["project"]["name"].value_or("Koi Application");
		desc.WindowDesc.Title  = projectFile["window"]["title"].value_or("Koi");
		desc.WindowDesc.Width  = projectFile["window"]["width"].value_or(1280u);
		desc.WindowDesc.Height = projectFile["window"]["height"].value_or(720u);
		desc.WindowDesc.VSync  = projectFile["window"]["vsync"].value_or(true);
		return std::make_unique<SandboxApp>(std::move(desc));
	}
}

void SandboxApp::OnInit()
{
	KOI_LOG_INFO("SandboxApp::OnInit()");
	KOI_MEM_TAG_SCOPE(koi::MemTag::User);
	CreatePipeline();
	LoadMesh();
	SetupCamera();
}

void SandboxApp::OnTick(koi::f32 dt)
{
	koi::Engine& engine       = *GetEngine();
	koi::InputManager& input  = engine.GetInputManager();
	koi::WindowHandle mainWnd = engine.GetWindowManager().GetMainWndowHandle();

	if (const koi::WindowInput* wi = input.GetWindowInput(mainWnd))
	{
		if (wi->IsKeyPressed(koi::KeyCode::Escape))
			engine.RequestExit();

		if (m_controller)
			m_controller->Update(dt, *wi);
	}
}

void SandboxApp::OnRender()
{
	koi::Engine& engine     = *GetEngine();
	koi::Renderer& renderer = engine.GetRenderer();
	koi::GfxDevice& device  = renderer.GetDevice();

	koi::RenderWindowHandle rwh = engine.GetMainRenderWindowHandle();
	koi::RenderWindow* rw       = renderer.GetRenderWindow(rwh);
	koi::CommandList* cmd       = renderer.GetCommandList(rwh);

	if (!rw || !cmd) return;

	const koi::u32 width  = rw->GetWidth();
	const koi::u32 height = rw->GetHeight();
	EnsureDepthBuffer(device, width, height);

	const koi::RendererResources& res = renderer.GetDefaultResources();

	cmd->ClearDepthStencil(m_depthTexture);
	cmd->SetRenderTarget(rw->GetHDRColorBuffer(), m_depthTexture);
	cmd->SetViewportAndScissor(width, height);
	cmd->SetPipeline(m_pipeline);
	cmd->SetIndexBuffer(m_mesh.IndexBuffer);

	PushConstants pc{};
	koi::math::store(pc.MVP, m_camera.ViewProjection);
	pc.VertexBufferIndex = m_mesh.VertexBufferSRV;
	pc.SamplerIndex      = res.LinearWrapSampler;

	for (const koi::GPUMeshDraw& d : m_mesh.DrawItems)
	{
		pc.BaseColorTextureIndex = d.BaseColorSRV;
		cmd->SetPushConstants(&pc, sizeof(pc) / sizeof(koi::u32));
		cmd->DrawIndexed(d.IndexCount, 1, d.IndexOffset, d.VertexOffset);
	}
}

void SandboxApp::OnShutdown()
{
	KOI_LOG_INFO("SandboxApp::OnShutdown()");
	koi::GfxDevice& device = GetEngine()->GetRenderer().GetDevice();
	device.Destroy(m_pipeline);
	m_mesh.Release(device);
	if (m_depthTexture)
		device.Destroy(m_depthTexture);
}

void SandboxApp::CreatePipeline()
{
	KOI_MEM_TAG_SCOPE(koi::MemTag::User);
	KOI_LOG_INFO("SandboxApp::CreatePipeline()");

	koi::GfxDevice& device = GetEngine()->GetRenderer().GetDevice();

	std::vector<byte> vsData = koi::LoadShaderBytecode("Assets/Shaders/MeshVS.cso");
	std::vector<byte> psData = koi::LoadShaderBytecode("Assets/Shaders/MeshPS.cso");
	assert((!vsData.empty() && !psData.empty()) && "Failed to load mesh shaders");

	m_pipeline = device.CreatePipeline(koi::PipelineDesc
	{
		.VertexShader        = vsData,
		.PixelShader         = psData,
		.Rasterizer          = { .CullMode = D3D12_CULL_MODE_NONE },
		.DepthStencil        = { .DepthEnable = true, .DepthWrite = true },
		.RenderTargetFormats = { koi::Format::R16G16B16A16_FLOAT },
		.RenderTargetCount   = 1,
		.DepthFormat         = kDepthFormat,
		.DebugName           = "MeshOpaque",
	});
}

void SandboxApp::LoadMesh()
{
	KOI_MEM_TAG_SCOPE(koi::MemTag::User);
	KOI_LOG_INFO("SandboxApp::LoadMesh()");

	m_model = koi::MeshLoader::Load("Assets/Models/DamagedHelmet.gltf");
	assert(m_model && !m_model->Meshes.empty() && "Failed to load DamagedHelmet.gltf");

	koi::Renderer& renderer  = GetEngine()->GetRenderer();
	koi::GfxDevice& device   = renderer.GetDevice();
	const koi::DescriptorIndex whiteSRV = device.Get(renderer.GetDefaultResources().WhiteTexture)->SRVIndex;

	m_mesh = koi::GPUMesh::Upload(device, *m_model, whiteSRV);
}

void SandboxApp::SetupCamera()
{
	KOI_MEM_TAG_SCOPE(koi::MemTag::User);
	koi::f32 mn[4]{}, mx[4]{};
	koi::math::store(mn, m_model->Bounds.Min);
	koi::math::store(mx, m_model->Bounds.Max);

	const koi::math::float3 center{ 0.5f * (mn[0] + mx[0]), 0.5f * (mn[1] + mx[1]), 0.5f * (mn[2] + mx[2]) };
	const koi::f32 ex = 0.5f * (mx[0] - mn[0]);
	const koi::f32 ey = 0.5f * (mx[1] - mn[1]);
	const koi::f32 ez = 0.5f * (mx[2] - mn[2]);
	const koi::f32 radius = std::max(std::sqrt(ex * ex + ey * ey + ez * ez), 0.001f);

	m_camera.SetProjection(0.8f, GetAspect(), 0.05f, 1000.0f);
	m_controller = std::make_unique<koi::CameraController>(&m_camera, koi::CameraMode::Orbit);
	m_controller->SetOrbitTarget(center, radius * 2.5f, 0.0f, 0.3f);
}

void SandboxApp::EnsureDepthBuffer(koi::GfxDevice& device, koi::u32 width, koi::u32 height)
{
	if (width == 0 || height == 0) return;
	if (m_depthTexture && width == m_depthWidth && height == m_depthHeight) return;

	if (m_depthTexture)
		device.Destroy(m_depthTexture);

	m_depthTexture = device.CreateTexture(koi::TextureDesc
	{
		.Width     = width,
		.Height    = height,
		.Format    = kDepthFormat,
		.Usage     = koi::TextureUsage::DepthStencil,
		.DebugName = "Scene Depth",
	});

	m_depthWidth  = width;
	m_depthHeight = height;
	m_camera.SetAspect(static_cast<koi::f32>(width) / static_cast<koi::f32>(height));
}

koi::f32 SandboxApp::GetAspect() const
{
	const koi::WindowDesc& w = GetDesc().WindowDesc;
	return w.Height > 0 ? static_cast<koi::f32>(w.Width) / static_cast<koi::f32>(w.Height) : 16.0f / 9.0f;
}
