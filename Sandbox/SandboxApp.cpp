#pragma once
#include "Application/AppBase.h"
#include "Engine/Engine.h"
#include "Graphics/Renderer.h"
#include "Graphics/RenderWindow.h"
#include "Graphics/RHI/Device.h"
#include "Graphics/RHI/CommandList.h"
#include "Graphics/RHI/PipelineDesc.h"
#include "Graphics/ShaderLoader.h"
#include <Kirin/IO/VirtualFileSystem.h>
#include <Kirin/IO/File.h>

using namespace krn::types;

struct Vertex
{
	float Position[3];
	float Color[4];
};

class SandboxApp : public koi::AppBase
{
public:
	SandboxApp(koi::AppCreateDesc desc) : AppBase(std::move(desc)) {}

	void OnInit() override
	{
		KOI_LOG_INFO("SandboxApp::OnInit()");

		koi::Engine& engine    = *GetEngine();
		koi::GfxDevice& device = engine.GetRenderer().GetDevice();

		// Load shader bytecode from the compiled output directory
		auto vsFileResult = krn::File::Open("Assets/Shaders/TriangleVS.cso");
		auto psFileResult = krn::File::Open("Assets/Shaders/TrianglePS.cso");

		KOI_ASSERT_MSG(vsFileResult.has_value() && psFileResult.has_value(), "Failed to load Hello Triangle shaders");

		const krn::File& vsFileData = vsFileResult.value();
		const krn::File& psFileData = psFileResult.value();

		// Vertex layout: POSITION (float3) + COLOR (float4)
		const koi::VertexAttribute vertexLayout[] =
		{
			{ "POSITION", 0, koi::Format::R32G32B32_FLOAT,    0, offsetof(Vertex, Position) },
			{ "COLOR",    0, koi::Format::R32G32B32A32_FLOAT, 0, offsetof(Vertex, Color)    },
		};

		koi::Format rtFormat = koi::Format::R8G8B8A8_UNORM;

		auto ToSpan = [](std::string_view data) -> std::span<const byte> { return { reinterpret_cast<const byte*>(data.data()), data.size() }; };

		koi::PipelineDesc pipelineDesc
		{
			.VertexLayout        = vertexLayout,
			.VertexShader        = ToSpan(vsFileData.Data()),
			.PixelShader         = ToSpan(psFileData.Data()),
			.DepthStencil        = { .DepthEnable = false },
			.RenderTargetFormats = { rtFormat },
			.RenderTargetCount   = 1,
			.DebugName           = "HelloTriangle",
		};

		m_pipeline = device.CreatePipeline(pipelineDesc);

		// Vertex data: NDC-space triangle with per-vertex colour
		const Vertex vertices[] =
		{
			{ {  0.0f,  0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },  // top    – red
			{ {  0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },  // right  – green
			{ { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },  // left   – blue
		};
		const u16 indices[] = { 0, 1, 2 };

		const u64 vbSize = sizeof(vertices);
		const u64 ibSize = sizeof(indices);

		m_vertexBuffer = device.CreateBuffer(koi::BufferDesc
		{
			.Size      = vbSize,
			.Stride    = sizeof(Vertex),
			.Usage     = koi::BufferUsage::Vertex | koi::BufferUsage::CopyDest,
			.Memory    = koi::MemoryUsage::GpuOnly,
			.DebugName = "Triangle VB",
		});

		m_indexBuffer = device.CreateBuffer(koi::BufferDesc
		{
			.Size      = ibSize,
			.Stride    = sizeof(u16),
			.Usage     = koi::BufferUsage::Index | koi::BufferUsage::CopyDest,
			.Memory    = koi::MemoryUsage::GpuOnly,
			.DebugName = "Triangle IB",
		});

		device.UploadBufferData(m_vertexBuffer, vertices, vbSize);
		device.UploadBufferData(m_indexBuffer,  indices,  ibSize);
	}

	void OnTick(koi::f32 /*dt*/) override {}

	void OnRender() override
	{
		koi::Engine& engine     = *GetEngine();
		koi::Renderer& renderer = engine.GetRenderer();

		koi::RenderWindowHandle rwh = engine.GetMainRenderWindowHandle();
		koi::RenderWindow* rw       = renderer.GetRenderWindow(rwh);
		koi::CommandList* cmd       = renderer.GetCommandList(rwh);

		if (!rw || !cmd) return;

		cmd->SetRenderTarget(rw->CurrentBackBuffer());
		cmd->SetViewportAndScissor(rw->GetWidth(), rw->GetHeight());
		cmd->SetPipeline(m_pipeline);
		cmd->SetVertexBuffer(0, m_vertexBuffer);
		cmd->SetIndexBuffer(m_indexBuffer);
		cmd->DrawIndexed(3);
	}

	void OnShutdown() override
	{
		KOI_LOG_INFO("SandboxApp::OnShutdown()");

		koi::GfxDevice& device = GetEngine()->GetRenderer().GetDevice();
		device.Destroy(m_pipeline);
		device.Destroy(m_vertexBuffer);
		device.Destroy(m_indexBuffer);
	}

private:
	koi::PipelineHandle m_pipeline;
	koi::BufferHandle   m_vertexBuffer;
	koi::BufferHandle   m_indexBuffer;
};

namespace koi
{
	std::unique_ptr<AppBase> CreateApplication(i32 argc, char** argv)
	{
		AppCreateDesc desc{};
		desc.AppName             = "Hello Triangle";
		desc.WindowDesc.Title    = "Koi - Hello Triangle";
		desc.WindowDesc.Width    = 1280;
		desc.WindowDesc.Height   = 720;
		desc.ArgC = argc;
		desc.ArgV = argv;
		return std::make_unique<SandboxApp>(std::move(desc));
	}
}
