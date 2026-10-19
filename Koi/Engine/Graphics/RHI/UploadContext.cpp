#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Core/Assert.h"
#include "Koi/Engine/Core/Patterns/Match.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Logging/Logger.h"
#include <algorithm>
#include <cstring>

namespace koi
{
	void GfxDevice::UploadContext::Init(GfxDevice* device)
	{
		ZoneScoped;

		KOI_ASSERT(device, "UploadContext::Init requires a valid GfxDevice");
		m_device = device;

		const D3D12_COMMAND_QUEUE_DESC queueDesc
		{
			.Type     = D3D12_COMMAND_LIST_TYPE_COPY,
			.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
			.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE,
			.NodeMask = 0,
		};
		DX_CHECK(m_device->m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_copyQueue)));
		m_copyQueue->SetName(L"Upload Copy Queue");

		DX_CHECK(m_device->m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&m_allocator)));
		m_allocator->SetName(L"Upload Allocator");

		DX_CHECK(m_device->m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, m_allocator.Get(), nullptr, IID_PPV_ARGS(&m_cmdList)));
		m_cmdList->SetName(L"Upload CmdList");
		DX_CHECK(m_cmdList->Close());

		DX_CHECK(m_device->m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_copyFence)));
		m_copyFence->SetName(L"Upload Copy Fence");

		m_fenceEvent = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
		if (!m_fenceEvent)
		{
			throw std::runtime_error("Failed to create the upload fence event");
		}

		KOI_LOG(RHI, Debug, "UploadContext initialized");
	}

	void GfxDevice::UploadContext::Shutdown()
	{
		ZoneScoped;

		KOI_ASSERT(m_recordDepth == 0, "UploadContext::Shutdown called while a Begin/End block is still open");

		// If there are staging buffers pending from the last FlushQueued, wait for the
		// copy fence to pass so we can safely destroy them.
		if (!m_pendingStagingFree.empty())
		{
			if (m_copyFence->GetCompletedValue() < m_pendingFreeFenceValue)
			{
				DX_CHECK(m_copyFence->SetEventOnCompletion(m_pendingFreeFenceValue, m_fenceEvent));
				::WaitForSingleObject(m_fenceEvent, INFINITE);
			}

			for (BufferHandle h : m_pendingStagingFree)
			{
				m_device->Destroy(h);
			}

			m_device->FlushPendingDeletions();
			m_pendingStagingFree.clear();
		}

		if (m_fenceEvent)
		{
			::CloseHandle(m_fenceEvent);
			m_fenceEvent = nullptr;
		}

		m_cmdList   = nullptr;
		m_allocator = nullptr;
		m_copyFence = nullptr;
		m_copyQueue = nullptr;

		KOI_LOG(RHI, Debug, "UploadContext shut down");
	}

	void GfxDevice::UploadContext::Begin()
	{
		ZoneScoped;

		KOI_ASSERT(m_device, "UploadContext::Begin called before Init");

		// Only the outermost Begin() can operate on the logic below (nested Begin() no-ops)
		if (m_recordDepth++ > 0) return;

		KOI_ASSERT(m_activeStagingBuffers.empty(), "Staging buffers leaked from a previous Begin/End block");

		DX_CHECK(m_allocator->Reset());
		DX_CHECK(m_cmdList->Reset(m_allocator.Get(), nullptr));
	}

	void GfxDevice::UploadContext::UploadBuffer(BufferHandle dst, const void* data, u64 size)
	{
		KOI_ASSERT(m_recordDepth > 0, "UploadBuffer called without a matching Begin()");
		KOI_ASSERT(data, "UploadBuffer: data pointer is null");
		KOI_ASSERT(size > 0, "UploadBuffer: size must be > 0");
		KOI_ASSERT(m_device->m_buffers.IsValid(dst), "UploadBuffer: invalid destination buffer handle");

		m_activeStagingBuffers.push_back(RecordBufferUpload(dst, data, size));
	}

	void GfxDevice::UploadContext::UploadTexture(TextureHandle dst, const void* data, u64 size)
	{
		KOI_ASSERT(m_recordDepth > 0, "UploadTexture called without a matching Begin()");
		KOI_ASSERT(data, "UploadTexture: data pointer is null");
		KOI_ASSERT(size > 0, "UploadTexture: size must be > 0");
		KOI_ASSERT(m_device->m_textures.IsValid(dst), "UploadTexture: invalid destination texture handle");

		m_activeStagingBuffers.push_back(RecordTextureUpload(dst, data, size));
	}

	void GfxDevice::UploadContext::UploadTextureSubresources(TextureHandle dst, std::span<const byte> data, std::span<const TextureSubresourceLayout> layout)
	{
		KOI_ASSERT(m_recordDepth > 0, "UploadTextureSubresources called without a matching Begin()");
		KOI_ASSERT(!data.empty() && !layout.empty(), "UploadTextureSubresources: nothing to upload");
		KOI_ASSERT(m_device->m_textures.IsValid(dst), "UploadTextureSubresources: invalid destination texture handle");

		m_activeStagingBuffers.push_back(RecordSubresourcesUpload(dst, data, layout));
	}

	void GfxDevice::UploadContext::End()
	{
		ZoneScoped;

		KOI_ASSERT(m_recordDepth > 0, "UploadContext::End called without a matching Begin()");

		// Inner batches defer submission (and the CPU stall) to the outermost End()
		if (--m_recordDepth > 0) return;

		DX_CHECK(m_cmdList->Close());

		if (m_activeStagingBuffers.empty())
			return;  // The list still had to be closed so the next Reset is legal


		dx12::CommandList* lists[] = { m_cmdList.Get() };
		m_copyQueue->ExecuteCommandLists(1, lists);

		const u64 fenceValue = m_nextFenceValue++;
		DX_CHECK(m_copyQueue->Signal(m_copyFence.Get(), fenceValue));
		DX_CHECK(m_copyFence->SetEventOnCompletion(fenceValue, m_fenceEvent));
		::WaitForSingleObject(m_fenceEvent, INFINITE);

		for (BufferHandle h : m_activeStagingBuffers)
		{
			m_device->Destroy(h);
		}

		m_device->FlushPendingDeletions();
		m_activeStagingBuffers.clear();
	}

	void GfxDevice::UploadContext::QueueBufferUpload(BufferHandle dst, const std::vector<byte>& data)
	{
		KOI_ASSERT(m_device, "UploadContext::QueueBufferUpload called before Init");
		KOI_ASSERT(m_recordDepth == 0, "Cannot queue an upload while a synchronous Begin/End block is open");
		KOI_ASSERT(m_device->m_buffers.IsValid(dst), "QueueBufferUpload: invalid destination buffer handle");
		KOI_ASSERT(!data.empty(), "QueueBufferUpload: data vector is empty");

		m_pendingUploads.push_back({ dst, data });
	}

	void GfxDevice::UploadContext::QueueTextureUpload(TextureHandle dst, const std::vector<byte>& data)
	{
		KOI_ASSERT(m_device, "UploadContext::QueueTextureUpload called before Init");
		KOI_ASSERT(m_recordDepth == 0, "Cannot queue an upload while a synchronous Begin/End block is open");
		KOI_ASSERT(m_device->m_textures.IsValid(dst), "QueueTextureUpload: invalid destination texture handle");
		KOI_ASSERT(!data.empty(), "QueueTextureUpload: data vector is empty");

		m_pendingUploads.push_back({ dst, data });
	}

	void GfxDevice::UploadContext::FlushQueued(dx12::CommandQueue* graphicsQueue)
	{
		ZoneScoped;

		KOI_ASSERT(m_device, "UploadContext::FlushQueued called before Init");
		KOI_ASSERT(m_recordDepth == 0, "FlushQueued called while a synchronous Begin/End block is open");
		KOI_ASSERT(graphicsQueue, "FlushQueued: graphicsQueue is null");

		// Ensure previous flush's staging buffers are freed before submitting new work.
		// The copy queue is fast relative to frame rate, so the poll branch is almost always taken.
		if (!m_pendingStagingFree.empty())
		{
			// Rare: copy queue somehow hasn't caught up in a whole frame —> actually block
			if (m_copyFence->GetCompletedValue() < m_pendingFreeFenceValue)
			{
				DX_CHECK(m_copyFence->SetEventOnCompletion(m_pendingFreeFenceValue, m_fenceEvent));
				::WaitForSingleObject(m_fenceEvent, INFINITE);
			}

			for (BufferHandle h : m_pendingStagingFree)
			{
				m_device->Destroy(h);
			}

			m_device->FlushPendingDeletions();
			m_pendingStagingFree.clear();
		}

		constexpr std::string_view kUploadCounter = "Upload KB";

		if (m_pendingUploads.empty())
		{
			FrameStats::Count(kUploadCounter, 0);
			return;
		}

		KOI_LOG(RHI, Trace, "UploadContext: flushing {} queued upload(s)", m_pendingUploads.size());

		DX_CHECK(m_allocator->Reset());
		DX_CHECK(m_cmdList->Reset(m_allocator.Get(), nullptr));

		std::vector<BufferHandle> stagingThisFlush;
		stagingThisFlush.reserve(m_pendingUploads.size());

		i64 uploadBytes = 0;
		for (PendingUpload& upload : m_pendingUploads)
		{
			uploadBytes += i64(upload.Data.size());
			std::visit(Match
			{
				[&](BufferHandle h)  { stagingThisFlush.push_back(RecordBufferUpload(h,  upload.Data.data(), upload.Data.size())); },
				[&](TextureHandle h) { stagingThisFlush.push_back(RecordTextureUpload(h, upload.Data.data(), upload.Data.size())); },
			}, upload.Dst);
		}
		m_pendingUploads.clear();
		FrameStats::Count(kUploadCounter, (uploadBytes + 1023) / 1024);

		DX_CHECK(m_cmdList->Close());

		dx12::CommandList* lists[] = { m_cmdList.Get() };
		m_copyQueue->ExecuteCommandLists(1, lists);

		const u64 signalValue = m_nextFenceValue++;
		DX_CHECK(m_copyQueue->Signal(m_copyFence.Get(), signalValue));
		m_pendingFreeFenceValue = signalValue;
		m_pendingStagingFree    = std::move(stagingThisFlush);

		// GPU-side ordering: graphics queue will not begin until the copy queue signals
		DX_CHECK(graphicsQueue->Wait(m_copyFence.Get(), signalValue));
	}

	BufferHandle GfxDevice::UploadContext::RecordBufferUpload(BufferHandle dst, const void* data, u64 size)
	{
		KOI_ASSERT(m_device->m_buffers.IsValid(dst), "RecordBufferUpload: destination buffer is not alive");

		BufferHandle staging = m_device->CreateBuffer(BufferDesc
		{
			.Size      = size,
			.Usage     = BufferUsage::CopySource,
			.Memory    = MemoryUsage::CpuToGpu,
			.DebugName = "Upload Staging Buffer",
		});

		Buffer* dstBuf     = m_device->m_buffers.Get(dst);
		Buffer* stagingBuf = m_device->m_buffers.Get(staging);

		KOI_ASSERT(dstBuf, "RecordBufferUpload: destination buffer is not alive after staging alloc");
		KOI_ASSERT(stagingBuf && stagingBuf->MappedPtr, "RecordBufferUpload: staging buffer mapping failed");

		std::memcpy(stagingBuf->MappedPtr, data, size);

		// Both resources are in COMMON/GENERIC_READ which copy queue accepts implicitly
		m_cmdList->CopyBufferRegion(dstBuf->Resource.Get(), 0, stagingBuf->Resource.Get(), 0, size);
		return staging;
	}

	BufferHandle GfxDevice::UploadContext::RecordTextureUpload(TextureHandle dst, const void* data, [[maybe_unused]] u64 size)
	{
		KOI_ASSERT(m_device->m_textures.IsValid(dst), "RecordTextureUpload: destination texture is not alive");

		const D3D12_RESOURCE_DESC rd = m_device->m_textures.Get(dst)->Resource->GetDesc();

		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
		u32 numRows        = 0;
		u64 rowSizeInBytes = 0;
		u64 totalBytes     = 0;
		m_device->m_device->GetCopyableFootprints(&rd, 0, 1, 0, &footprint, &numRows, &rowSizeInBytes, &totalBytes);

		KOI_ASSERT(size >= rowSizeInBytes * numRows, "RecordTextureUpload: source data is smaller than texture mip 0");

		BufferHandle staging = m_device->CreateBuffer(BufferDesc
		{
			.Size      = totalBytes,
			.Usage     = BufferUsage::CopySource,
			.Memory    = MemoryUsage::CpuToGpu,
			.DebugName = "Texture Upload Staging",
		});

		Texture* tex       = m_device->m_textures.Get(dst);
		Buffer* stagingBuf = m_device->m_buffers.Get(staging);
		KOI_ASSERT(tex && tex->Resource, "RecordTextureUpload: destination texture is not alive after staging alloc");
		KOI_ASSERT(stagingBuf && stagingBuf->MappedPtr, "RecordTextureUpload: staging buffer mapping failed");

		// Row-by-row copy: source is tightly packed, destination pitch is aligned to D3D12 requirements
		byte* dstBytes       = static_cast<byte*>(stagingBuf->MappedPtr) + footprint.Offset;
		const byte* srcBytes = static_cast<const byte*>(data);

		for (u32 row = 0; row < numRows; ++row)
		{
			std::memcpy(dstBytes + row * footprint.Footprint.RowPitch, srcBytes + row * rowSizeInBytes, rowSizeInBytes);
		}

		const CD3DX12_TEXTURE_COPY_LOCATION dstLoc(tex->Resource.Get(), 0);
		const CD3DX12_TEXTURE_COPY_LOCATION srcLoc(stagingBuf->Resource.Get(), footprint);
		m_cmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

		return staging;
	}

	BufferHandle GfxDevice::UploadContext::RecordSubresourcesUpload(TextureHandle dst, std::span<const byte> data, std::span<const TextureSubresourceLayout> layout)
	{
		ZoneScoped;

		const D3D12_RESOURCE_DESC rd = m_device->m_textures.Get(dst)->Resource->GetDesc();
		const u32 count = static_cast<u32>(layout.size());
		KOI_ASSERT(count == static_cast<u32>(rd.MipLevels) * rd.DepthOrArraySize, "RecordSubresourcesUpload: layout doesn't cover every subresource");

		std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(count);
		std::vector<u32> numRows(count);
		std::vector<u64> rowBytes(count);
		u64 totalBytes = 0;
		m_device->m_device->GetCopyableFootprints(&rd, 0, count, 0, footprints.data(), numRows.data(), rowBytes.data(), &totalBytes);

		BufferHandle staging = m_device->CreateBuffer(BufferDesc
		{
			.Size      = totalBytes,
			.Usage     = BufferUsage::CopySource,
			.Memory    = MemoryUsage::CpuToGpu,
			.DebugName = "Texture Upload Staging",
		});

		Texture* tex       = m_device->m_textures.Get(dst);
		Buffer* stagingBuf = m_device->m_buffers.Get(staging);
		KOI_ASSERT(stagingBuf && stagingBuf->MappedPtr, "RecordSubresourcesUpload: staging buffer mapping failed");

		byte* mapped = static_cast<byte*>(stagingBuf->MappedPtr);

		bool sameLayout = totalBytes == data.size();
		for (u32 i = 0; i < count && sameLayout; ++i)
		{
			sameLayout = footprints[i].Offset == layout[i].Offset
				&& footprints[i].Footprint.RowPitch == layout[i].RowPitch
				&& numRows[i] == layout[i].NumRows;
		}

		if (sameLayout)
		{
			std::memcpy(mapped, data.data(), data.size());  // the whole point of the .ktex layout
		}
		else
		{
			KOI_LOG(RHI, Debug, "Texture layout differs from this GPU's footprints, copying it row by row");
			for (u32 i = 0; i < count; ++i)
			{
				const u32 rows  = std::min(numRows[i], layout[i].NumRows);
				const u64 bytes = std::min<u64>(rowBytes[i], layout[i].RowBytes);
				for (u32 row = 0; row < rows; ++row)
				{
					std::memcpy(mapped + footprints[i].Offset + static_cast<u64>(row) * footprints[i].Footprint.RowPitch,
						data.data() + layout[i].Offset + static_cast<u64>(row) * layout[i].RowPitch, bytes);
				}
			}
		}

		for (u32 i = 0; i < count; ++i)
		{
			const CD3DX12_TEXTURE_COPY_LOCATION dstLoc(tex->Resource.Get(), i);
			const CD3DX12_TEXTURE_COPY_LOCATION srcLoc(stagingBuf->Resource.Get(), footprints[i]);
			m_cmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);
		}

		return staging;
	}
}
