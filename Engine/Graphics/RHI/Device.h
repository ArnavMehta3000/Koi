#include "Graphics/DX12/DX12Common.h"
#include "Graphics/RHI/DescriptorHeap.h"
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
		explicit GfxDevice(const GfxDevice::Desc& desc);
		~GfxDevice();

		[[nodiscard]] dx12::Device* GetNative() const noexcept              { return m_device.Get();        }
		[[nodiscard]] dxgi::Factory* GetDXGIFactory() const noexcept        { return m_dxgiFactory.Get();   }
		[[nodiscard]] dx12::CommandQueue* GetGraphicsQueue() const noexcept { return m_graphicsQueue.Get(); }
		[[nodiscard]] dx12::Fence* GetFrameFence() const noexcept           { return m_frameeFence.Get();   }
		[[nodiscard]] bool SupportsTearing() const noexcept                 { return m_supportsTearing;     }
		[[nodiscard]] u64 GetCurrentFrameIndex() const noexcept             { return m_frameIndex;          }
		[[nodiscard]] u64 GetCompletedFenceValue() const noexcept;

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
			enum class Kind : u8 { Texture, Buffer };
			Kind Type;
			u64  DenseGen;       // handle generation at time of queue (unused for now, future-proofing)
			u64  FenceValue;
			union
			{
				TextureHandle Texture;
				BufferHandle  Buffer;
			};
			PendingDeletion() {}
		};

		IDeviceNotify*              m_notify = nullptr;

		ComPtr<dxgi::Factory>       m_dxgiFactory;
		ComPtr<dxgi::Adapter>       m_adapter;
		ComPtr<dx12::Device>        m_device;
		ComPtr<dx12::CommandQueue>  m_graphicsQueue;
		ComPtr<dx12::Fence>         m_frameFence;
		HANDLE                      m_fenceEvent      = nullptr;

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
