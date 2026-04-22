#include "Graphics/DX12/DX12Common.h"
#include "Graphics/RHI/DescriptorHeap.h"
#include "Graphics/RHI/Resources.h"
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

		[[nodiscard]] inline dx12::Device* GetNative() const noexcept{ return m_device.Get (); }
		[[nodiscard]] inline dxgi::Factory* GetDXGIFactory() const noexcept{ return m_dxgiFactory.Get (); }
		[[nodiscard]] inline dx12::CommandQueue* GetGraphicsQueue() const noexcept{ return m_graphicsQueue.Get(); }
		[[nodiscard]] inline dx12::Fence* GetFrameFence() const noexcept{ return m_frameFence.Get (); }
		[[nodiscard]] inline bool SupportsTearing() const noexcept{ return m_supportsTearing; }
		[[nodiscard]] inline u64 GetCurrentFrameIndex() const noexcept{ return m_frameIndex; }
		[[nodiscard]] inline DescriptorHeap& RTVHeap() noexcept{ return m_rtvHeap; }
		[[nodiscard]] inline DescriptorHeap& DSVHeap() noexcept{ return m_dsvHeap; }
		[[nodiscard]] inline DescriptorHeap& ResourceHeap() noexcept{ return m_resourceHeap; }
		[[nodiscard]] inline DescriptorHeap& SamplerHeap() noexcept{ return m_samplerHeap; }
		[[nodiscard]] u64 GetCompletedFenceValue () const noexcept;

		[[nodiscard]] Texture* Get(TextureHandle h) noexcept              { return m_textures.Get(h); }
		[[nodiscard]] const Texture* Get(TextureHandle h) const noexcept  { return m_textures.Get(h); }
		[[nodiscard]] Buffer* Get(BufferHandle h) noexcept                { return m_buffers.Get(h);  }
		[[nodiscard]] const Buffer* Get(BufferHandle h) const noexcept    { return m_buffers.Get(h);  }

		[[nodiscard]] TextureHandle RegisterSwapChainImage(ComPtr<dx12::Resource> resource, const TextureDesc& desc);

		[[nodiscard]] TextureHandle CreateTexture(const TextureDesc& desc);
		[[nodiscard]] BufferHandle CreateBuffer(const BufferDesc& desc);

		void Destroy(TextureHandle h);
		void Destroy(BufferHandle h);

		u64 SignalFrame();
		void WaitForFence(u64 fenceValue);
		void WaitForIdle();  // Block until GPU has completed all subbed in work

		void SetDeviceNotify(IDeviceNotify* notify) noexcept { m_notify = notify; }
		void HandleDeviceLost();

	private:
		void CreateFactory(bool enableDebug);
		void PickAdapter();
		void CreateDevice();
		void CreateQueues();
		void CreateFences();
		void CreateHeaps();
		void FlushDeferredDeletions(u64 completedFence);

	private:
		struct PendingDeletion
		{
			enum class ResourceType : u8 { Texture, Buffer };
			ResourceType Type;
			u64 DenseGen;       // handle generation at time of queue (unused for now, future-proofing)
			u64 FenceValue;
			union
			{
				TextureHandle Texture;
				BufferHandle  Buffer;
			};
			PendingDeletion() {}
		};

		IDeviceNotify*             m_notify = nullptr;

		ComPtr<dx12::Device>       m_device;
		ComPtr<dxgi::Factory>      m_dxgiFactory;
		ComPtr<dxgi::Adapter>      m_adapter;
		ComPtr<dx12::CommandQueue> m_graphicsQueue;
		ComPtr<dx12::Fence>        m_frameFence;
		HANDLE                     m_fenceEvent = nullptr;

		DescriptorHeap m_rtvHeap;
		DescriptorHeap m_dsvHeap;
		DescriptorHeap m_resourceHeap;   // CBV/SRV/UAV shader-visible bindless table
		DescriptorHeap m_samplerHeap;

		HandlePool<Texture>         m_textures;
		HandlePool<Buffer>          m_buffers;

		std::queue<PendingDeletion> m_deletionQueue;

		u64                         m_nextFenceValue  = 1;
		u64                         m_frameIndex      = 0;
		bool                        m_supportsTearing = false;
		Desc                        m_desc;
	};
}
