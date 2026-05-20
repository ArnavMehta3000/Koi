#pragma once
#include "Runtime/AppBase.h"
#include "Runtime/Engine.h"
#include "Runtime/CommandLine.h"
#include "Graphics/Renderer.h"
#include "Graphics/RenderWindow.h"
#include "Graphics/RHI/Device.h"
#include "Graphics/RHI/CommandList.h"
#include "Graphics/RHI/PipelineDesc.h"
#include "Graphics/ShaderLoader.h"
#include "Graphics/MeshLoader.h"
#include "Memory/MemoryDebug.h"
#include "Core/Math.h"
#include <cassert>
#include <toml++/toml.hpp>

struct Vertex
{
	koi::math::float3 Position;
	koi::math::float4 Color;
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
		std::vector<byte> vsData = koi::LoadShaderBytecode("Assets/Shaders/TriangleVS.cso");
		std::vector<byte> psData = koi::LoadShaderBytecode("Assets/Shaders/TrianglePS.cso");

		assert((!vsData.empty() && !psData.empty()) && "Failed to load Hello Triangle shaders");

		const std::array<koi::VertexAttribute, 2> vertexLayout =
		{
			koi::VertexAttribute
			{
				.SemanticName  = "POSITION",
				.SemanticIndex = 0,
				.Format        = koi::Format::R32G32B32_FLOAT,
				.InputSlot     = 0,
				.ByteOffset    = offsetof(Vertex, Position),
				.PerInstance   = false
			},
			{
				.SemanticName  = "COLOR",
				.SemanticIndex = 0,
				.Format        = koi::Format::R32G32B32A32_FLOAT,
				.InputSlot     = 0,
				.ByteOffset    = offsetof(Vertex, Color),
				.PerInstance   = false
			}
		};

		koi::Format rtFormat = koi::Format::R8G8B8A8_UNORM;

		koi::PipelineDesc pipelineDesc
		{
			.VertexLayout        = vertexLayout,
			.VertexShader        = vsData,
			.PixelShader         = psData,
			.DepthStencil        = { .DepthEnable = false },
			.RenderTargetFormats = { rtFormat },
			.RenderTargetCount   = 1,
			.DebugName           = "HelloTriangle",
		};

		m_pipeline = device.CreatePipeline(pipelineDesc);

		// Vertex data: NDC-space triangle with per-vertex colour
		const std::array<Vertex, 3> vertices =
		{
			Vertex
			{ {  0.0f,  0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },  // top    – red
			{ {  0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },  // right  – green
			{ { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },  // left   – blue
		};

		constexpr  std::array<koi::u16, 3> indices = { 0, 1, 2 };

		constexpr koi::u64 vbSize = sizeof(vertices);
		constexpr koi::u64 ibSize = sizeof(indices);

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
			.Stride    = sizeof(koi::u16),
			.Usage     = koi::BufferUsage::Index | koi::BufferUsage::CopyDest,
			.Memory    = koi::MemoryUsage::GpuOnly,
			.DebugName = "Triangle IB",
		});

		device.UploadBufferData(m_vertexBuffer, vertices.data(), vbSize);
		device.UploadBufferData(m_indexBuffer,  indices.data(),  ibSize);

	}

	void OnTick(koi::f32 /*dt*/) override
	{
		koi::Engine& engine       = *GetEngine();
		koi::InputManager& input  = engine.GetInputManager();
		koi::WindowHandle mainWnd = engine.GetWindowManager().GetMainWndowHandle();

		if (const koi::WindowInput* wi = input.GetWindowInput(mainWnd))
		{
			if (wi->IsKeyPressed(koi::KeyCode::Escape))
				engine.RequestExit();

			if (wi->IsMouseButtonPressed(koi::MouseButton::Left))
			{
				KOI_LOG_INFO("Left clicked");
			}
		}
	}

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

// Custom commandline parser
class CmdLine : public koi::CommandLine
{
public:
	CmdLine() : koi::CommandLine("Koi Sandbox") {}

private:
	koi::i32 m_textVal = 0;
};
static koi::CommandLineRegistrar<CmdLine> s_cmdReg;

namespace koi
{
	std::unique_ptr<AppBase> CreateApplication(CommandLine& cmd)
	{
		//auto& child = *static_cast<CmdLine*>(&cmd);

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
