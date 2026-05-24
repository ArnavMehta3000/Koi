#pragma once
#include "Runtime/AppBase.h"
#include "Graphics/Renderer.h"
#include "Graphics/RHI/Device.h"  // For upload context
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

// One DrawIndexed per submesh into the shared vertex/index buffers.
struct DrawItem
{
	koi::u32 IndexCount;
	koi::u32 IndexOffset;
	koi::i32 VertexOffset;
	koi::u32 BaseColorTextureIndex;
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

	// TODO: Cleanup
	static D3D12_SAMPLER_DESC LinearWrapSampler();
	static koi::TextureHandle CreateImageTexture(koi::GfxDevice& device, koi::GfxDevice::UploadContext& upload, const koi::ImageAsset& img);
	static koi::TextureHandle CreateSolidTexture(koi::GfxDevice& device, koi::GfxDevice::UploadContext& upload, koi::u32 rgba);


private:
	std::unique_ptr<koi::ModelAsset>       m_model;
	koi::PipelineHandle                    m_pipeline;
	koi::BufferHandle                      m_vertexBuffer;
	koi::BufferHandle                      m_indexBuffer;
	koi::TextureHandle                     m_depthTexture;
	koi::u32                               m_depthWidth = 0;
	koi::u32                               m_depthHeight = 0;
	koi::DescriptorIndex                   m_vertexBufferIndex = koi::INVALID_DESCRIPTOR;
	koi::DescriptorIndex                   m_sampler = koi::INVALID_DESCRIPTOR;

	koi::TextureHandle                     m_whiteTexture;
	std::vector<koi::TextureHandle>        m_materialTextures;

	std::vector<DrawItem>                  m_draws;

	koi::Camera                            m_camera;
	std::unique_ptr<koi::CameraController> m_controller;
};
