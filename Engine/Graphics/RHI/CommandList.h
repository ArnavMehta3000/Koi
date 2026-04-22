#pragma once
#include "Graphics/Types.h"
#include "Graphics/DX12/DX12Common.h"

namespace koi
{
	class GfxDevice;
	class CommandAllocator;

	class CommandList : NonCopyable
	{
	public:
		CommandList() = default;
		~CommandList() = default;

		[[nodiscard]] inline dx12::GraphicsCommandList* GetNative() const noexcept { return m_cmdList.Get(); }
		[[nodiscard]] inline dx12::CommandList* GetNativeBase() const noexcept { return m_cmdList.Get(); }

		void Init(GfxDevice& device, CommandAllocator& allocator, D3D12_COMMAND_LIST_TYPE type, std::wstring_view debugName = L"");

		void Begin(u32 frameSlot);
		void End();

		void TransitionBarrier(TextureHandle h, D3D12_RESOURCE_STATES newState);
		void TransitionBarrier(BufferHandle  h, D3D12_RESOURCE_STATES newState);
		void FlushBarriers();

		void ClearRenderTarget(TextureHandle rt, const f32 color[4]);

	private:
		GfxDevice*                          m_device    = nullptr;
		CommandAllocator*                   m_allocator = nullptr;
		D3D12_COMMAND_LIST_TYPE             m_type      = D3D12_COMMAND_LIST_TYPE_DIRECT;
		bool                                m_recording = false;
		ComPtr<dx12::GraphicsCommandList>   m_cmdList;
		std::vector<D3D12_RESOURCE_BARRIER> m_pendingBarriers;
	};
}
