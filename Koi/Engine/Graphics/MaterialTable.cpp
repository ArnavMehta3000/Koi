#include "Koi/Engine/Graphics/MaterialTable.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Memory/MemoryTracker.h"
#include <cassert>

namespace koi
{
	void MaterialTable::Init(GfxDevice& device, const GPUMaterialData& fallback)
	{
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);

		m_buffer.Init(device, BufferUsage::Storage, 16, "Material Table");

		const u32 slot = Allocate(fallback);
		assert(slot == kFallbackSlot && "The fallback has to be the first row allocated");
	}

	void MaterialTable::Shutdown()
	{
		m_buffer.Shutdown();
		m_rows.clear();
		m_free.clear();
	}

	u32 MaterialTable::Allocate(const GPUMaterialData& row)
	{
		if (!m_free.empty())
		{
			const u32 slot = m_free.back();
			m_free.pop_back();
			m_rows[slot] = row;
			return slot;
		}

		m_rows.push_back(row);
		return static_cast<u32>(m_rows.size() - 1);
	}

	void MaterialTable::Set(u32 slot, const GPUMaterialData& row)
	{
		if (slot < m_rows.size()) m_rows[slot] = row;
	}

	void MaterialTable::Free(u32 slot)
	{
		if (slot == kFallbackSlot || slot >= m_rows.size()) return;

		// A freed row keeps drawing until something claims the slot, so it becomes the fallback
		m_rows[slot] = m_rows[kFallbackSlot];
		m_free.push_back(slot);
	}

	DescriptorIndex MaterialTable::Upload(u32 frameSlot)
	{
		// Tiny enough that a wholesale copy costs less than tracking what changed
		return m_buffer.Upload(frameSlot, std::span<const GPUMaterialData>(m_rows));
	}
}
