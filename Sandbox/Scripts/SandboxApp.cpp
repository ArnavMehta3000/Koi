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
#include "Graphics/Camera.h"
#include "Graphics/CameraController.h"
#include "Memory/MemoryDebug.h"
#include "Core/Math.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <memory>
#include <vector>
#include <toml++/toml.hpp>

class SandboxApp : public koi::AppBase
{
	// Mirrors the HLSL b0 root-constant layout (MVP + bindless vertex buffer index).
	struct PushConstants
	{
		koi::f32 MVP[16];
		koi::u32 VertexBufferIndex;
	};

	// One DrawIndexed per submesh into the shared vertex/index buffers.
	struct DrawItem
	{
		koi::u32 IndexCount;
		koi::u32 IndexOffset;
		koi::i32 VertexOffset;
	};

public:
	SandboxApp(koi::AppCreateDesc desc) : AppBase(std::move(desc)) {}

	void OnInit() override
	{
		KOI_LOG_INFO("SandboxApp::OnInit()");

		koi::Engine& engine    = *GetEngine();
		koi::GfxDevice& device = engine.GetRenderer().GetDevice();

		// Pipeline
		std::vector<byte> vsData = koi::LoadShaderBytecode("Assets/Shaders/MeshVS.cso");
		std::vector<byte> psData = koi::LoadShaderBytecode("Assets/Shaders/MeshPS.cso");
		assert((!vsData.empty() && !psData.empty()) && "Failed to load mesh shaders");

		koi::PipelineDesc pipelineDesc
		{
			.VertexShader        = vsData,  // No IA vertex layout: vertices are pulled bindlessly
			.PixelShader         = psData,
			.Rasterizer          = { .CullMode = D3D12_CULL_MODE_NONE },
			.DepthStencil        = { .DepthEnable = true, .DepthWrite = true },
			.RenderTargetFormats = { koi::Format::R8G8B8A8_UNORM },
			.RenderTargetCount   = 1,
			.DepthFormat         = kDepthFormat,
			.DebugName           = "MeshOpaque",
		};
		m_pipeline = device.CreatePipeline(pipelineDesc);

		// Mesh
		std::unique_ptr<koi::ModelAsset> model = koi::MeshLoader::Load("Assets/Models/DamagedHelmet.gltf");
		assert(model && !model->Meshes.empty() && "Failed to load DamagedHelmet.gltf");

		std::vector<koi::Vertex> vertices;
		std::vector<koi::u32> indices;
		for (const koi::Mesh& mesh : model->Meshes)
		{
			const koi::u32 baseVertex = static_cast<koi::u32>(vertices.size());
			const koi::u32 baseIndex  = static_cast<koi::u32>(indices.size());

			vertices.insert(vertices.end(), mesh.Vertices.begin(), mesh.Vertices.end());
			indices.insert(indices.end(), mesh.Indices.begin(), mesh.Indices.end());

			for (const koi::SubMesh& sub : mesh.SubMeshes)
			{
				m_draws.push_back(DrawItem
				{
					.IndexCount   = sub.IndexCount,
					.IndexOffset  = baseIndex + sub.IndexOffset,
					.VertexOffset = static_cast<koi::i32>(baseVertex + sub.VertexOffset),
				});
			}
		}

		const koi::u64 vbSize = vertices.size() * sizeof(koi::Vertex);
		const koi::u64 ibSize = indices.size() * sizeof(koi::u32);

		// Vertex buffer is a structured (Storage) buffer so it gets a bindless SRV.
		m_vertexBuffer = device.CreateBuffer(koi::BufferDesc
		{
			.Size      = vbSize,
			.Stride    = sizeof(koi::Vertex),
			.Usage     = koi::BufferUsage::Storage | koi::BufferUsage::CopyDest,
			.Memory    = koi::MemoryUsage::GpuOnly,
			.DebugName = "Helmet VB",
		});

		m_indexBuffer = device.CreateBuffer(koi::BufferDesc
		{
			.Size      = ibSize,
			.Stride    = sizeof(koi::u32),
			.Usage     = koi::BufferUsage::Index | koi::BufferUsage::CopyDest,
			.Memory    = koi::MemoryUsage::GpuOnly,
			.DebugName = "Helmet IB",
		});

		device.UploadBufferData(m_vertexBuffer, vertices.data(), vbSize);
		device.UploadBufferData(m_indexBuffer,  indices.data(),  ibSize);

		m_vertexBufferIndex = device.Get(m_vertexBuffer)->SRVIndex;
		assert(m_vertexBufferIndex != koi::INVALID_DESCRIPTOR && "Vertex buffer has no bindless SRV");

		// Camera
		koi::f32 mn[4]{}, mx[4]{};
		koi::math::store(mn, model->Bounds.Min);
		koi::math::store(mx, model->Bounds.Max);

		const koi::math::float3 center{ 0.5f * (mn[0] + mx[0]), 0.5f * (mn[1] + mx[1]), 0.5f * (mn[2] + mx[2]) };
		const koi::f32 ex = 0.5f * (mx[0] - mn[0]);
		const koi::f32 ey = 0.5f * (mx[1] - mn[1]);
		const koi::f32 ez = 0.5f * (mx[2] - mn[2]);
		const koi::f32 radius = std::max(std::sqrt(ex * ex + ey * ey + ez * ez), 0.001f);

		m_camera.SetProjection(0.8f, GetAspect(), 0.05f, 1000.0f);
		m_controller = std::make_unique<koi::CameraController>(&m_camera, koi::CameraMode::Orbit);
		m_controller->SetOrbitTarget(center, radius * 2.5f, 0.0f, 0.3f);
	}

	void OnTick(koi::f32 dt) override
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

	void OnRender() override
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

		cmd->ClearDepthStencil(m_depthTexture);
		cmd->SetRenderTarget(rw->CurrentBackBuffer(), m_depthTexture);
		cmd->SetViewportAndScissor(width, height);
		cmd->SetPipeline(m_pipeline);
		cmd->SetIndexBuffer(m_indexBuffer);

		// Model matrix is identity for now, so MVP == camera ViewProjection.
		PushConstants pc{ .VertexBufferIndex = m_vertexBufferIndex };
		koi::math::store(pc.MVP, m_camera.ViewProjection);
		cmd->SetPushConstants(&pc, sizeof(pc) / sizeof(koi::u32));

		for (const DrawItem& d : m_draws)
			cmd->DrawIndexed(d.IndexCount, 1, d.IndexOffset, d.VertexOffset);
	}

	void OnShutdown() override
	{
		KOI_LOG_INFO("SandboxApp::OnShutdown()");

		koi::GfxDevice& device = GetEngine()->GetRenderer().GetDevice();
		device.Destroy(m_pipeline);
		device.Destroy(m_vertexBuffer);
		device.Destroy(m_indexBuffer);
		if (m_depthTexture)
			device.Destroy(m_depthTexture);
	}

private:
	[[nodiscard]] koi::f32 GetAspect() const
	{
		const koi::WindowDesc& w = GetDesc().WindowDesc;
		return w.Height > 0 ? static_cast<koi::f32>(w.Width) / static_cast<koi::f32>(w.Height) : 16.0f / 9.0f;
	}

	// (Re)creates the depth buffer when the window size changes and keeps the camera aspect in sync.
	void EnsureDepthBuffer(koi::GfxDevice& device, koi::u32 width, koi::u32 height)
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

	static constexpr koi::Format kDepthFormat = koi::Format::D32_FLOAT;

	koi::PipelineHandle  m_pipeline;
	koi::BufferHandle    m_vertexBuffer;
	koi::BufferHandle    m_indexBuffer;
	koi::TextureHandle   m_depthTexture;
	koi::u32             m_depthWidth        = 0;
	koi::u32             m_depthHeight       = 0;
	koi::DescriptorIndex m_vertexBufferIndex = koi::INVALID_DESCRIPTOR;

	std::vector<DrawItem> m_draws;

	koi::Camera m_camera;
	std::unique_ptr<koi::CameraController> m_controller;
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
