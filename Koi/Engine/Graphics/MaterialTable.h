#pragma once
#include "Koi/Engine/Content/Shaders/Interop/Mesh.hlsli"
#include "Koi/Engine/RHI/GpuFrameBuffer.h"
#include <vector>

namespace koi
{
	class GfxDevice;

	class MaterialTable : public NonCopyable
	{
	public:
		static constexpr u32 kFallbackSlot = 0;

		void Init(GfxDevice& device, const GPUMaterialData& fallback);
		void Shutdown();

		[[nodiscard]] u32 Allocate(const GPUMaterialData& row);
		void Set(u32 slot, const GPUMaterialData& row);
		void Free(u32 slot);

		// Copies the table into this frame's buffer and returns its bindless index
		DescriptorIndex Upload(u32 frameSlot);

		[[nodiscard]] u32 Count() const { return static_cast<u32>(m_rows.size() - m_free.size()); }

	private:
		std::vector<GPUMaterialData>    m_rows;
		std::vector<u32>                m_free;
		GpuFrameBuffer<GPUMaterialData> m_buffer;
	};
}
