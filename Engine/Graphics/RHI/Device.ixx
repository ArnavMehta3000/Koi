module;
#include "Core/Config.h"
#include "Platform/Win32.h"
#include <wrl/client.h>
#define D3DX12_USE_ARRAY_COM_PTR
#include <directx/d3dx12.h>
#include <dxgi1_6.h>
#include <queue>
#include <variant>
#include <vector>
export module Koi.Graphics.RHI.Device;

import Koi.Graphics.Types;
import Koi.Graphics.RHI.DX12Common;
import Koi.Graphics.RHI.DescriptorHeap;
import Koi.Graphics.RHI.Resources;
import Koi.Graphics.RHI.PipelineDesc;

export namespace koi
{
	class GfxDevice
	{
	public:
		struct Desc
		{
			bool EnableDebugLayer   = KOI_DEBUG;
			bool EnableGPUValidation = false;
		};

		class UploadContext : NonCopyable, NonMovable
		{
		public:
			void Init(GfxDevice* device);
			void Shutdown();

			void Begin();
			void UploadBuffer(BufferHandle dst, const void* data, u64 size);
			void UploadTexture(TextureHandle dst, const void* data, u64 size);
			void End();

			void QueueBufferUpload(BufferHandle dst, const std::vector<byte>& data);
			void QueueTextureUpload(TextureHandle dst, const std::vector<byte>& data);
			void FlushQueued(dx12::CommandQueue* graphicsQueue);

		private:
			struct PendingUpload
			{
				std::variant<BufferHandle, TextureHandle> Dst;
				std::vector<byte> Data;
			};

			[[nodiscard]] BufferHandle RecordBufferUpload(BufferHandle dst, const void* data, u64 size);
			[[nodiscard]] BufferHandle RecordTextureUpload(TextureHandle dst, const void* data, u64 size);

			GfxDevice*                        m_device               = nullptr;
			ComPtr<dx12::CommandQueue>        m_copyQueue;
			ComPtr<dx12::CommandAllocator>    m_allocator;
			ComPtr<dx12::GraphicsCommandList> m_cmdList;
			ComPtr<dx12::Fence>               m_copyFence;
			HANDLE                            m_fenceEvent           = nullptr;
			u64                               m_nextFenceValue       = 1;
			std::vector<PendingUpload>        m_pendingUploads;
			std::vector<BufferHandle>         m_activeStagingBuffers;
			std::vector<BufferHandle>         m_pendingStagingFree;
			u64                               m_pendingFreeFenceValue = 0;
			bool                              m_isRecording          = false;
		};

	public:
		explicit GfxDevice(const GfxDevice::Desc& desc = {});
		~GfxDevice();

		[[nodiscard]] dx12::Device*        GetNative()            const noexcept { return m_device.Get();        }
		[[nodiscard]] dxgi::Factory*       GetDXGIFactory()       const noexcept { return m_dxgiFactory.Get();   }
		[[nodiscard]] dx12::CommandQueue*  GetGraphicsQueue()     const noexcept { return m_graphicsQueue.Get(); }
		[[nodiscard]] dx12::Fence*         GetFrameFence()        const noexcept { return m_frameFence.Get();    }
		[[nodiscard]] dx12::RootSignature* GetRootSignature()     const noexcept { return m_rootSignature.Get(); }
		[[nodiscard]] bool                 SupportsTearing()      const noexcept { return m_supportsTearing;     }
		[[nodiscard]] u64                  GetCurrentFrameIndex() const noexcept { return m_frameIndex;          }
		[[nodiscard]] DescriptorHeap&      RTVHeap()              noexcept       { return m_rtvHeap;             }
		[[nodiscard]] DescriptorHeap&      DSVHeap()              noexcept       { return m_dsvHeap;             }
		[[nodiscard]] DescriptorHeap&      ResourceHeap()         noexcept       { return m_resourceHeap;        }
		[[nodiscard]] DescriptorHeap&      SamplerHeap()          noexcept       { return m_samplerHeap;         }
		[[nodiscard]] UploadContext&       GetUploadContext()     noexcept       { return m_uploadContext;       }

		[[nodiscard]] u64 GetCompletedFenceValue() const noexcept;

		[[nodiscard]] Texture*        Get(TextureHandle h)  noexcept       { return m_textures.Get(h);  }
		[[nodiscard]] const Texture*  Get(TextureHandle h)  const noexcept { return m_textures.Get(h);  }
		[[nodiscard]] Buffer*         Get(BufferHandle h)   noexcept       { return m_buffers.Get(h);   }
		[[nodiscard]] const Buffer*   Get(BufferHandle h)   const noexcept { return m_buffers.Get(h);   }
		[[nodiscard]] Pipeline*       Get(PipelineHandle h) noexcept       { return m_pipelines.Get(h); }
		[[nodiscard]] const Pipeline* Get(PipelineHandle h) const noexcept { return m_pipelines.Get(h); }

		[[nodiscard]] TextureHandle  RegisterSwapChainImage(ComPtr<dx12::Resource> resource, const TextureDesc& desc);
		[[nodiscard]] TextureHandle  CreateTexture(const TextureDesc& desc);
		[[nodiscard]] BufferHandle   CreateBuffer(const BufferDesc& desc);
		[[nodiscard]] PipelineHandle CreatePipeline(const PipelineDesc& desc);
		[[nodiscard]] DescriptorIndex CreateSampler(const D3D12_SAMPLER_DESC& desc);

		void Destroy(TextureHandle h);
		void Destroy(BufferHandle h);
		void Destroy(PipelineHandle h);
		void FreeResource(TextureHandle h);
		void FreeResource(BufferHandle h);

		u64 SignalFrame();
		void WaitForFence(u64 fenceValue);
		void WaitForIdle();
		void FlushPendingDeletions();

		void SetDeviceNotify(IDeviceNotify* notify) noexcept { m_notify = notify; }
		void HandleDeviceLost();

	private:
		void CreateFactory(bool enableDebug);
		void PickAdapter();
		void CreateDevice();
		void CreateQueues();
		void CreateFences();
		void CreateHeaps();
		void CreateRootSignature();
		void FlushDeferredDeletions(u64 completedFence);

	private:
		struct PendingDeletion
		{
			u64 FenceValue;
			std::variant<TextureHandle, BufferHandle, PipelineHandle> Handle;
		};

		IDeviceNotify*              m_notify = nullptr;
		ComPtr<dx12::Device>        m_device;
		ComPtr<dxgi::Factory>       m_dxgiFactory;
		ComPtr<dxgi::Adapter>       m_adapter;
		ComPtr<dx12::CommandQueue>  m_graphicsQueue;
		ComPtr<dx12::Fence>         m_frameFence;
		ComPtr<dx12::RootSignature> m_rootSignature;
		HANDLE                      m_fenceEvent = nullptr;

		DescriptorHeap              m_rtvHeap;
		DescriptorHeap              m_dsvHeap;
		DescriptorHeap              m_resourceHeap;
		DescriptorHeap              m_samplerHeap;

		HandlePool<Texture>         m_textures;
		HandlePool<Buffer>          m_buffers;
		HandlePool<Pipeline>        m_pipelines;

		std::queue<PendingDeletion> m_deletionQueue;
		UploadContext               m_uploadContext;

		u64                         m_nextFenceValue  = 1;
		u64                         m_frameIndex      = 0;
		bool                        m_supportsTearing = false;
		Desc                        m_desc;
	};
}
