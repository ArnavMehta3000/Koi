#pragma once
#include "Runtime/AppBase.h"
#include "Graphics/Renderer.h"
#include "Graphics/RHI/Device.h"
#include "Graphics/GPUMesh.h"
#include "Graphics/Camera.h"
#include "Graphics/CameraController.h"
#include <memory>

// Mirrors the HLSL b0 root-constant layout (MVP + bindless descriptor indices).
struct PushConstants
{
	koi::f32 MVP[16];
	koi::u32 VertexBufferIndex;
	koi::u32 BaseColorTextureIndex;
	koi::u32 SamplerIndex;
};

class SandboxApp : public koi::AppBase
{
	static constexpr koi::Format kDepthFormat = koi::Format::D32_FLOAT;
public:
	explicit SandboxApp(koi::AppCreateDesc desc) : AppBase(std::move(desc)) {}

	void OnInit() override;
	void OnTick(koi::f32 dt) override;
	void OnRender() override;
	void OnShutdown() override;

private:
	void CreatePipeline();
	void LoadMesh();
	void SetupCamera();
	void EnsureDepthBuffer(koi::GfxDevice& device, koi::u32 width, koi::u32 height);
	koi::f32 GetAspect() const;

private:
	koi::PipelineHandle                    m_pipeline;
	koi::GPUMesh                           m_mesh;
	koi::TextureHandle                     m_depthTexture;
	koi::u32                               m_depthWidth  = 0;
	koi::u32                               m_depthHeight = 0;

	std::unique_ptr<koi::ModelAsset>       m_model;
	koi::Camera                            m_camera;
	std::unique_ptr<koi::CameraController> m_controller;
};
