#pragma once
#include "Koi/Engine/Graphics/Types.h"
#include <span>
#include <string>
#include <string_view>

namespace koi
{
	class GfxDevice;

	class GpuFrameBufferBase : public NonCopyable
	{
	public:
		GpuFrameBufferBase() = default;
		~GpuFrameBufferBase();

		void Init(GfxDevice& device, BufferUsage usage, u32 elementSize, u32 initialCount, std::string_view debugName);
		void Shutdown();

		// Bindless index for this frame slot, or INVALID_DESCRIPTOR before the first upload.
		[[nodiscard]] DescriptorIndex GetDescriptorIndex(u32 frameSlot) const;
		[[nodiscard]] BufferHandle    GetBuffer(u32 frameSlot) const { return m_buffers[frameSlot]; }
		[[nodiscard]] u32             GetCapacity(u32 frameSlot) const { return m_capacity[frameSlot]; }

	protected:
		// Copies `count` elements into this frame slot's buffer, growing it first if needed | Returns the bindless index
		DescriptorIndex UploadBytes(u32 frameSlot, const void* data, u32 count);

	private:
		void Reserve(u32 frameSlot, u32 count);

		GfxDevice*               m_device       = nullptr;
		BufferUsage              m_usage        = BufferUsage::None;
		u32                      m_elementSize  = 0;
		u32                      m_initialCount = 0;
		std::string              m_debugName;

		FrameArray<BufferHandle> m_buffers{};
		FrameArray<u32>          m_capacity{};
	};

	// A CPU-written, GPU-read buffer with one independent copy per frame in flight
	// BufferUsage::Storage  -> StructuredBuffer<T>; grows on demand, exposes the SRV index
	// BufferUsage::Constant -> ConstantBuffer<T>;   always one element, exposes the CBV index
	template<typename T>
	class GpuFrameBuffer : public GpuFrameBufferBase
	{
	public:
		void Init(GfxDevice& device, BufferUsage usage, u32 initialCount, std::string_view debugName)
		{
			GpuFrameBufferBase::Init(device, usage, sizeof(T), initialCount, debugName);
		}

		DescriptorIndex Upload(u32 frameSlot, std::span<const T> data)
		{
			return UploadBytes(frameSlot, data.data(), static_cast<u32>(data.size()));
		}

		DescriptorIndex Upload(u32 frameSlot, const T& value)
		{
			return UploadBytes(frameSlot, &value, 1);
		}
	};
}
