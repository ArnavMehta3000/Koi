#include "Graphics/RHI/CommandList.h"
#include "Graphics/RHI/CommandAllocator.h"
#include "Graphics/RHI/Device.h"
#include "Graphics/RHI/Resources.h"
#include "Graphics/RHI/DescriptorHeap.h"
#include <cassert>

#pragma warning(push)
#pragma warning(disable: 6011)  // warning C6011: Dereferencing NULL pointer (I handle this with asserts instead)

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
		assert(!m_recording);
		assert(m_allocator && m_cmdList);

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
		assert(m_recording);
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

		assert(tex && tex->Resource && "Trying to clear an invalid texture");
		assert(tex->RTVIndex != INVALID_DESCRIPTOR && "Texture had no attached RTV");

		TransitionBarrier(rt, D3D12_RESOURCE_STATE_RENDER_TARGET);
		FlushBarriers();

		const auto cpuHandle = m_device->RTVHeap().CpuHandle(tex->RTVIndex);
		m_cmdList->ClearRenderTargetView(cpuHandle, color, 0, nullptr);
	}

	void CommandList::SetRenderTarget(TextureHandle colorRT, TextureHandle depthRT)
	{
		FlushBarriers();

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle{};
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle{};

		if (Texture* tex = m_device->Get(colorRT))
		{
			assert(tex->RTVIndex != INVALID_DESCRIPTOR && "SetRenderTarget: invalid color RT");
			rtvHandle = m_device->RTVHeap().CpuHandle(tex->RTVIndex);
		}

		if (Texture* tex = m_device->Get(depthRT))
		{
			assert(tex->DSVIndex != INVALID_DESCRIPTOR && "SetRenderTarget: invalid depth RT");
			dsvHandle = m_device->DSVHeap().CpuHandle(tex->DSVIndex);
		}

		m_cmdList->OMSetRenderTargets(
			colorRT ? 1 : 0,
			colorRT ? &rtvHandle : nullptr,
			FALSE,
			depthRT ? &dsvHandle : nullptr);
	}

	void CommandList::SetViewport(f32 x, f32 y, f32 width, f32 height, f32 minDepth, f32 maxDepth)
	{
		D3D12_VIEWPORT vp = { x, y, width, height, minDepth, maxDepth };
		m_cmdList->RSSetViewports(1, &vp);
	}

	void CommandList::SetScissor(u32 x, u32 y, u32 width, u32 height)
	{
		D3D12_RECT rect = { static_cast<LONG>(x), static_cast<LONG>(y),
		                    static_cast<LONG>(x + width), static_cast<LONG>(y + height) };
		m_cmdList->RSSetScissorRects(1, &rect);
	}

	void CommandList::SetViewportAndScissor(u32 width, u32 height)
	{
		SetViewport(0.0f, 0.0f, static_cast<f32>(width), static_cast<f32>(height));
		SetScissor(0, 0, width, height);
	}

	void CommandList::SetPipeline(PipelineHandle h)
	{
		Pipeline* pipeline = m_device->Get(h);
		assert((pipeline && pipeline->Pso) && "SetPipeline: invalid pipeline handle");

		m_cmdList->SetPipelineState(pipeline->Pso.Get());
		m_cmdList->SetGraphicsRootSignature(m_device->GetRootSignature());
		m_cmdList->IASetPrimitiveTopology(pipeline->Topology);
	}

	void CommandList::SetVertexBuffer(u32 slot, BufferHandle h)
	{
		Buffer* buf = m_device->Get(h);
		assert((buf && buf->Resource) && "SetVertexBuffer: invalid buffer");
		assert((buf->Desc.Stride > 0) && "SetVertexBuffer: buffer has no stride");

		D3D12_VERTEX_BUFFER_VIEW view
		{
			.BufferLocation = buf->GPUAddress,
			.SizeInBytes    = static_cast<u32>(buf->Desc.Size),
			.StrideInBytes  = buf->Desc.Stride,
		};
		m_cmdList->IASetVertexBuffers(slot, 1, &view);
	}

	void CommandList::SetIndexBuffer(BufferHandle h)
	{
		Buffer* buf = m_device->Get(h);
		assert((buf && buf->Resource) && "SetIndexBuffer: invalid buffer");

		const DXGI_FORMAT fmt = (buf->Desc.Stride == sizeof(u16))
			? DXGI_FORMAT_R16_UINT
			: DXGI_FORMAT_R32_UINT;

		D3D12_INDEX_BUFFER_VIEW view
		{
			.BufferLocation = buf->GPUAddress,
			.SizeInBytes    = static_cast<u32>(buf->Desc.Size),
			.Format         = fmt,
		};
		m_cmdList->IASetIndexBuffer(&view);
	}

	void CommandList::SetPushConstants(const void* data, u32 count32BitValues, u32 destOffsetIn32BitValues)
	{
		m_cmdList->SetGraphicsRoot32BitConstants(0, count32BitValues, data, destOffsetIn32BitValues);
	}

	void CommandList::Draw(u32 vertexCount, u32 instanceCount, u32 startVertex, u32 startInstance)
	{
		FlushBarriers();
		m_cmdList->DrawInstanced(vertexCount, instanceCount, startVertex, startInstance);
	}

	void CommandList::DrawIndexed(u32 indexCount, u32 instanceCount, u32 startIndex, i32 vertexOffset, u32 startInstance)
	{
		FlushBarriers();
		m_cmdList->DrawIndexedInstanced(indexCount, instanceCount, startIndex, vertexOffset, startInstance);
	}

	void CommandList::CopyBuffer(BufferHandle dst, u64 dstOffset, BufferHandle src, u64 srcOffset, u64 size)
	{
		Buffer* dstBuf = m_device->Get(dst);
		Buffer* srcBuf = m_device->Get(src);
		assert((dstBuf && srcBuf) && "CopyBuffer: invalid buffer handle(s)");

		FlushBarriers();
		m_cmdList->CopyBufferRegion(
			dstBuf->Resource.Get(), dstOffset,
			srcBuf->Resource.Get(), srcOffset,
			size);
	}
}

#pragma warning(pop)
