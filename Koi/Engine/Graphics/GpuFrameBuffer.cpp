#include "Koi/Engine/Graphics/GpuFrameBuffer.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Graphics/RHI/Resources.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include <algorithm>
#include <cassert>
#include <cstring>

namespace koi
{
	GpuFrameBufferBase::~GpuFrameBufferBase()
	{
		Shutdown();
	}

	void GpuFrameBufferBase::Init(GfxDevice& device, BufferUsage usage, u32 elementSize, u32 initialCount, std::string_view debugName)
	{
		assert(!m_device && "GpuFrameBuffer is already initialized");
		assert(elementSize > 0 && "GpuFrameBuffer element size must be non-zero");
		assert((EnumFlag(usage).HasAny(BufferUsage::Storage | BufferUsage::Constant))
			&& "GpuFrameBuffer supports Storage (StructuredBuffer) or Constant (ConstantBuffer) only");

		m_device       = &device;
		m_usage        = usage;
		m_elementSize  = elementSize;
		m_initialCount = std::max(1u, initialCount);
		m_debugName    = debugName;
	}

	void GpuFrameBufferBase::Shutdown()
	{
		if (!m_device) return;

		for (u32 i = 0; i < FRAMES_IN_FLIGHT; ++i)
		{
			if (m_buffers[i])
				m_device->Destroy(m_buffers[i]);

			m_buffers[i]  = BufferHandle();
			m_capacity[i] = 0;
		}

		m_device = nullptr;
	}

	DescriptorIndex GpuFrameBufferBase::GetDescriptorIndex(u32 frameSlot) const
	{
		if (!m_device) return INVALID_DESCRIPTOR;

		const Buffer* buf = m_device->Get(m_buffers[frameSlot]);
		if (!buf) return INVALID_DESCRIPTOR;

		return (EnumFlag(m_usage).Has(BufferUsage::Storage)) ? buf->SRVIndex : buf->CBVIndex;
	}

	void GpuFrameBufferBase::Reserve(u32 frameSlot, u32 count)
	{
		ZoneScoped;
		if (count <= m_capacity[frameSlot]) return;

		// Amortized growth, same idea as std::vector, so a per-frame count that creeps upwards
		// doesn't reallocate every frame.
		const u32 newCapacity = std::max(count, std::max(m_initialCount, m_capacity[frameSlot] * 2));

		if (m_buffers[frameSlot])
			m_device->Destroy(m_buffers[frameSlot]);

		m_buffers[frameSlot] = m_device->CreateBuffer(BufferDesc
		{
			.Size      = static_cast<u64>(newCapacity) * m_elementSize,
			.Stride    = (EnumFlag(m_usage).Has(BufferUsage::Storage)) ? m_elementSize : 0,
			.Usage     = EnumFlag(m_usage),
			.Memory    = MemoryUsage::CpuToGpu,
			.DebugName = m_debugName,
		});
		m_capacity[frameSlot] = newCapacity;
	}

	DescriptorIndex GpuFrameBufferBase::UploadBytes(u32 frameSlot, const void* data, u32 count)
	{
		ZoneScoped;
		assert(m_device && "GpuFrameBuffer used before Init()");
		assert(frameSlot < FRAMES_IN_FLIGHT && "Frame slot out of range");
		assert(!(EnumFlag(m_usage).Has(BufferUsage::Constant) && count > 1) && "Constant buffers hold a single element");

		if (count == 0) return INVALID_DESCRIPTOR;

		Reserve(frameSlot, count);

		Buffer* buf = m_device->Get(m_buffers[frameSlot]);
		assert(buf && buf->Resource && buf->MappedPtr && "Invalid GpuFrameBuffer buffer");

		std::memcpy(buf->MappedPtr, data, static_cast<size_t>(count) * m_elementSize);

		return (EnumFlag(m_usage).Has(BufferUsage::Storage)) ? buf->SRVIndex : buf->CBVIndex;
	}
}
