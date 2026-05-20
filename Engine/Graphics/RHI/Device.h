#pragma once
#include "Graphics/RHI/DX12Common.h"
#include "Graphics/RHI/DescriptorHeap.h"
#include "Graphics/RHI/Resources.h"
#include "Graphics/RHI/PipelineDesc.h"
#include <queue>

namespace koi
{
	class GfxDevice
	{
	public:
		struct Desc
		{
			bool EnableDebugLayer = KOI_DEBUG;
			bool EnableGPUValidation = false;
		};

	public:
		explicit GfxDevice(const GfxDevice::Desc& desc = {});
		~GfxDevice();

		[[nodiscard]] inline dx12::Device*        GetNative()            const noexcept { return m_device.Get ();       }
		[[nodiscard]] inline dxgi::Factory*       GetDXGIFactory()       const noexcept { return m_dxgiFactory.Get ();  }
		[[nodiscard]] inline dx12::CommandQueue*  GetGraphicsQueue()     const noexcept { return m_graphicsQueue.Get(); }
		[[nodiscard]] inline dx12::Fence*         GetFrameFence()        const noexcept { return m_frameFence.Get ();   }
		[[nodiscard]] inline dx12::RootSignature* GetRootSignature()     const noexcept { return m_rootSignature.Get(); }
		[[nodiscard]] inline bool                 SupportsTearing()      const noexcept { return m_supportsTearing;     }
		[[nodiscard]] inline u64                  GetCurrentFrameIndex() const noexcept { return m_frameIndex;          }
		[[nodiscard]] inline DescriptorHeap&      RTVHeap()              noexcept       { return m_rtvHeap;             }
		[[nodiscard]] inline DescriptorHeap&      DSVHeap()              noexcept       { return m_dsvHeap;             }
		[[nodiscard]] inline DescriptorHeap&      ResourceHeap()         noexcept       { return m_resourceHeap;        }
		[[nodiscard]] inline DescriptorHeap&      SamplerHeap()          noexcept       { return m_samplerHeap;         }

		[[nodiscard]] u64 GetCompletedFenceValue () const noexcept;

		[[nodiscard]] inline Texture*        Get(TextureHandle h)  noexcept       { return m_textures.Get(h);  }
		[[nodiscard]] inline const Texture*  Get(TextureHandle h)  const noexcept { return m_textures.Get(h);  }
		[[nodiscard]] inline Buffer*         Get(BufferHandle h)   noexcept       { return m_buffers.Get(h);   }
		[[nodiscard]] inline const Buffer*   Get(BufferHandle h)   const noexcept { return m_buffers.Get(h);   }
		[[nodiscard]] inline Pipeline*       Get(PipelineHandle h) noexcept       { return m_pipelines.Get(h); }
		[[nodiscard]] inline const Pipeline* Get(PipelineHandle h) const noexcept { return m_pipelines.Get(h); }

		[[nodiscard]] TextureHandle  RegisterSwapChainImage(ComPtr<dx12::Resource> resource, const TextureDesc& desc);

		[[nodiscard]] TextureHandle  CreateTexture(const TextureDesc& desc);
		[[nodiscard]] BufferHandle   CreateBuffer(const BufferDesc& desc);
		[[nodiscard]] PipelineHandle CreatePipeline(const PipelineDesc& desc);

		// Uploads CPU data to a GPU-only buffer via a one-shot command list. Stalls the CPU
		void UploadBufferData(BufferHandle dst, const void* data, u64 size);

		// Uploads tightly-packed CPU pixels to a GPU texture (mip 0, array slice 0). Stalls the CPU.
		// Leaves the texture in a shader-readable state.
		void UploadTextureData(TextureHandle dst, const void* data, u64 size);

		// Creates a sampler in the shader-visible sampler heap; returns its bindless index.
		[[nodiscard]] DescriptorIndex CreateSampler(const D3D12_SAMPLER_DESC& desc);

		// Mark resource to be defer deleted
		void Destroy(TextureHandle h);
		void Destroy(BufferHandle h);
		void Destroy(PipelineHandle h);

		// Removes resources from descriptor heaps
		void FreeResource(TextureHandle h);
		void FreeResource(BufferHandle h);

		u64 SignalFrame();
		void WaitForFence(u64 fenceValue);
		void WaitForIdle();           // Block until GPU has completed all submitted work
		void FlushPendingDeletions(); // Free all queued resources; caller must ensure GPU is idle

		inline void SetDeviceNotify(IDeviceNotify* notify) noexcept { m_notify = notify; }
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
			enum class ResourceType : u8 { Texture, Buffer, Pipeline };
			ResourceType Type;
			u64 DenseGen;
			u64 FenceValue;
			union
			{
				TextureHandle  Texture;
				BufferHandle   Buffer;
				PipelineHandle Pipeline;
			};
			PendingDeletion() {}
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
		DescriptorHeap              m_resourceHeap;   // CBV/SRV/UAV shader-visible bindless table
		DescriptorHeap              m_samplerHeap;

		HandlePool<Texture>         m_textures;
		HandlePool<Buffer>          m_buffers;
		HandlePool<Pipeline>        m_pipelines;

		std::queue<PendingDeletion> m_deletionQueue;

		u64                         m_nextFenceValue  = 1;
		u64                         m_frameIndex      = 0;
		bool                        m_supportsTearing = false;
		Desc                        m_desc;
	};
}
