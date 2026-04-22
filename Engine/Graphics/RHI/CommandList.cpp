#include "Graphics/RHI/CommandList.h"
#include "Graphics/RHI/CommandAllocator.h"
#include "Graphics/RHI/Device.h"
#include "Graphics/RHI/Resources.h"
#include "Graphics/RHI/DescriptorHeap.h"

namespace koi
{
	void CommandList::Init(GfxDevice& device, CommandAllocator& allocator, D3D12_COMMAND_LIST_TYPE type, std::wstring_view debugName)
	{
		m_device = &device;
		m_allocator = &allocator;
		m_type = type;

		DX_CHECK(device.GetNative()->CreateCommandList(
			0, type, allocator.Get(0), nullptr, IID_PPV_ARGS(&m_cmdList)));

		// Created in the recording state; close it so Begin() can reopen uniformly.
		DX_CHECK(m_cmdList->Close());

		if (!debugName.empty())
			m_cmdList->SetName(debugName.data());

		m_pendingBarriers.reserve(16);
	}

	void CommandList::Begin(u32 frameSlot)
	{
		KOI_ASSERT(!m_recording);
		KOI_ASSERT(m_allocator && m_cmdList);

		m_allocator->Reset(frameSlot);
		DX_CHECK(m_cmdList->Reset(m_allocator->Get(frameSlot), nullptr));

		// Bind shader-visible heaps so SRV/UAV/CBV and sampler descriptors are usable.
		const std::array heaps =
		{
			m_device->ResourceHeap().GetHeap(),
			m_device->SamplerHeap().GetHeap(),
		};
		m_cmdList->SetDescriptorHeaps(static_cast<u32>(heaps.size()), heaps.data());

		m_recording = true;
	}

	void CommandList::End()
	{
		KOI_ASSERT(m_recording);
		FlushBarriers();
		DX_CHECK(m_cmdList->Close());
		m_recording = false;
	}

	void CommandList::TransitionBarrier(TextureHandle h, D3D12_RESOURCE_STATES newState)
	{
		Texture* tex = m_device->Get(h);

		if (!tex || !tex->Resource) return;
		if (tex->CurrentState == newState) return;

		m_pendingBarriers.push_back(
			CD3DX12_RESOURCE_BARRIER::Transition(
				tex->Resource.Get(),
				tex->CurrentState,
				newState,
				D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
				D3D12_RESOURCE_BARRIER_FLAG_NONE));


		tex->CurrentState = newState;
	}

	void CommandList::TransitionBarrier(BufferHandle h, D3D12_RESOURCE_STATES newState)
	{
		Buffer* buf = m_device->Get(h);

		if (!buf || !buf->Resource) return;
		if (buf->CurrentState == newState) return;

		m_pendingBarriers.push_back(
			CD3DX12_RESOURCE_BARRIER::Transition(
				buf->Resource.Get(),
				buf->CurrentState,
				newState,
				D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
				D3D12_RESOURCE_BARRIER_FLAG_NONE));

		buf->CurrentState = newState;
	}

	void CommandList::FlushBarriers()
	{
		if (m_pendingBarriers.empty()) return;

		m_cmdList->ResourceBarrier(static_cast<u32>(m_pendingBarriers.size()), m_pendingBarriers.data());
		m_pendingBarriers.clear();
	}

	void CommandList::ClearRenderTarget(TextureHandle rt, const f32 color[4])
	{
		Texture* tex = m_device->Get(rt);

		KOI_ASSERT_MSG(tex && tex->Resource, "Trying to clear an invalid texture");
		KOI_ASSERT_MSG(tex->RTVIndex != INVALID_DESCRIPTOR, "Texture had no attached RTV");

		TransitionBarrier(rt, D3D12_RESOURCE_STATE_RENDER_TARGET);
		FlushBarriers();

		const auto cpuHandle = m_device->RTVHeap().CpuHandle(tex->RTVIndex);
		m_cmdList->ClearRenderTargetView(cpuHandle, color, 0, nullptr);
	}
}
