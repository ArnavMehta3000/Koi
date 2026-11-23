#pragma once
#include "Koi/Engine/RHI/DX12Common.h"
#include "Koi/Engine/RHI/GpuProfiling.h"
#include "Koi/Engine/RHI/DescriptorHeap.h"
#include "Koi/Engine/RHI/Resources.h"
#include "Koi/Engine/RHI/PipelineDesc.h"
#include <expected>
#include <queue>
#include <span>
#include <variant>
#include <vector>

namespace koi
{
	class GfxDevice
	{
	public:
		struct Desc
		{
			bool EnableDebugLayer = false;
			bool EnableGPUValidation = false;
		};

		class UploadContext : NonCopyable, NonMovable
		{
		public:
			void Init(GfxDevice* device);
			void Shutdown();

			// Synchronous batch -> CPU stalls on End()
			void Begin();
			void UploadBuffer(BufferHandle dst, const void* data, u64 size);
			void UploadTexture(TextureHandle dst, const void* data, u64 size);

			// Every subresource at once, from data already laid out as described (a .ktex payload)
			void UploadTextureSubresources(TextureHandle dst, std::span<const byte> data, std::span<const TextureSubresourceLayout> layout);
			void End();

		private:
			[[nodiscard]] BufferHandle RecordBufferUpload(BufferHandle dst, const void* data, u64 size);
			[[nodiscard]] BufferHandle RecordTextureUpload(TextureHandle dst, const void* data, u64 size);
			[[nodiscard]] BufferHandle RecordSubresourcesUpload(TextureHandle dst, std::span<const byte> data, std::span<const TextureSubresourceLayout> layout);

			GfxDevice*                        m_device               = nullptr;
			ComPtr<dx12::CommandQueue>        m_copyQueue;
			ComPtr<dx12::CommandAllocator>    m_allocator;
			ComPtr<dx12::GraphicsCommandList> m_cmdList;
			ComPtr<dx12::Fence>               m_copyFence;
			HANDLE                            m_fenceEvent           = nullptr;
			u64                               m_nextFenceValue       = 1;

			std::vector<BufferHandle>         m_activeStagingBuffers;
			u32                               m_recordDepth          = 0;  // Final End() actually uploads (allows nesting)
		};

	public:
		explicit GfxDevice(const GfxDevice::Desc& desc);
		~GfxDevice();

		[[nodiscard]] inline dx12::Device*        GetNative()            const noexcept { return m_device.Get ();       }
		[[nodiscard]] inline dxgi::Factory*       GetDXGIFactory()       const noexcept { return m_dxgiFactory.Get ();  }
		[[nodiscard]] inline dx12::CommandQueue*  GetGraphicsQueue()     const noexcept { return m_graphicsQueue.Get(); }
		[[nodiscard]] inline dx12::Fence*         GetFrameFence()        const noexcept { return m_frameFence.Get ();   }
		[[nodiscard]] inline dx12::RootSignature* GetRootSignature()     const noexcept { return m_rootSignature.Get(); }
		[[nodiscard]] inline bool                 SupportsTearing()      const noexcept { return m_supportsTearing;     }
		[[nodiscard]] inline u64                  GetCurrentFrameIndex() const noexcept { return m_frameIndex;          }
		[[nodiscard]] inline u32                  GetFrameSlot()         const noexcept { return static_cast<u32>(m_frameIndex % FRAMES_IN_FLIGHT); }
		[[nodiscard]] inline DescriptorHeap&      RTVHeap()              noexcept       { return m_rtvHeap;             }
		[[nodiscard]] inline DescriptorHeap&      DSVHeap()              noexcept       { return m_dsvHeap;             }
		[[nodiscard]] inline DescriptorHeap&      ResourceHeap()         noexcept       { return m_resourceHeap;        }
		[[nodiscard]] inline DescriptorHeap&      SamplerHeap()          noexcept       { return m_samplerHeap;         }
		[[nodiscard]] inline UploadContext&       GetUploadContext()     noexcept       { return m_uploadContext;       }
		[[nodiscard]] inline TracyD3D12Ctx        GetProfilerCtx()       const noexcept { return m_gpuProfilerCtx;      }
		[[nodiscard]] inline GpuTimers&           GetGpuTimers()         noexcept       { return m_gpuTimers;           }

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
		[[nodiscard]] PipelineHandle CreatePipeline(const ComputePipelineDesc& desc);

		// Rebuilds an existing pipeline in place | call at a frame boundary: the old PSO is released immediately,
		// so no command list may still be recording draws against it
		bool RecreatePipeline(PipelineHandle h, const PipelineDesc& desc);
		bool RecreatePipeline(PipelineHandle h, const ComputePipelineDesc& desc);

		// Creates a sampler in the shader-visible sampler heap; returns its bindless index.
		[[nodiscard]] DescriptorIndex CreateSampler(const D3D12_SAMPLER_DESC& desc);

		// Mark resource to be defer deleted
		void Destroy(TextureHandle h);
		void Destroy(BufferHandle h);
		void Destroy(PipelineHandle h);

		// Removes resources from descriptor heaps
		void FreeResource(TextureHandle h);
		void FreeResource(BufferHandle h);

		// Closes off the previous frame's timestamp payload / drains any that the GPU has finished.
		// Must stay 1:1 per frame: NewFrame signals the graphics queue, Collect reads the results back.
		void GpuProfilerNewFrame();
		void GpuProfilerCollect();

		u64 SignalFrame();
		void WaitForFence(u64 fenceValue);
		void WaitForIdle();           // Block until GPU has completed all submitted work
		void FlushPendingDeletions(); // Free all queued resources; caller must ensure GPU is idle


	private:
		void CreateFactory(bool enableDebug);
		void PickAdapter();
		void CreateDevice();
		void CreateQueues();
		void CreateFences();
		void CreateHeaps();
		void CreateRootSignature();
		[[nodiscard]] std::expected<ComPtr<dx12::PipelineState>, HRESULT> BuildPipelineState(const PipelineDesc& desc);
		[[nodiscard]] std::expected<ComPtr<dx12::PipelineState>, HRESULT> BuildPipelineState(const ComputePipelineDesc& desc);
		[[nodiscard]] std::expected<ComPtr<dx12::PipelineState>, HRESULT> CreateFromStream(const D3D12_PIPELINE_STATE_STREAM_DESC& stream, std::string_view name);
		void FlushDeferredDeletions(u64 completedFence);
		void ReportLiveResources();

	private:
		struct PendingDeletion
		{
			u64 FenceValue;
			std::variant<TextureHandle, BufferHandle, PipelineHandle> Handle;
		};

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
		UploadContext               m_uploadContext;

		TracyD3D12Ctx               m_gpuProfilerCtx  = nullptr;
		GpuTimers                   m_gpuTimers;

		u64                         m_nextFenceValue  = 1;
		u64                         m_frameIndex      = 0;
		bool                        m_supportsTearing = false;
		Desc                        m_desc;
	};
}
