#include "Koi/Engine/Graphics/RenderGraph/TransientPool.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Text/StringUtils.h"
#include <algorithm>
#include <cassert>

namespace koi
{
	namespace
	{
		constexpr u64 kEvictAfterFrames = 30;  // Long enough to ride out a window drag without recreating every frame
	}

	void TransientPool::Init(GfxDevice& device)
	{
		m_device = &device;
	}

	void TransientPool::Shutdown()
	{
		if (!m_device) return;

		for (const Entry& e : m_entries)
		{
			m_device->Destroy(e.Texture);
		}
		m_entries.clear();
	}

	bool TransientPool::Matches(const TextureDesc& a, const TextureDesc& b) noexcept
	{
		return a.Width        == b.Width
			&& a.Height       == b.Height
			&& a.DepthOrArray == b.DepthOrArray
			&& a.MipLevels    == b.MipLevels
			&& a.SampleCount  == b.SampleCount
			&& a.Format       == b.Format
			&& a.Usage        == b.Usage;
	}

	TextureHandle TransientPool::Acquire(const TextureDesc& desc)
	{
		ZoneScoped;
		assert(m_device && "TransientPool used before Init");

		auto it = std::ranges::find_if(m_entries, [&](const Entry& e) { return !e.InUse && Matches(e.Desc, desc); });
		if (it == m_entries.end())
		{
			Entry& e = m_entries.emplace_back(Entry
			{
				.Desc    = desc,
				.Name    = std::string(desc.DebugName),
				.Texture = m_device->CreateTexture(desc),
			});
			e.Desc.DebugName = {};

			KOI_LOG(RHI, Trace, "Transient texture created: '{}' {}x{}", e.Name, desc.Width, desc.Height);
			it = std::prev(m_entries.end());
		}
		else if (it->Name != desc.DebugName)
		{
			// Recycled under another name, rename it
			it->Name = std::string(desc.DebugName);
			if (Texture* tex = m_device->Get(it->Texture))
			{
				tex->Resource->SetName(utils::ToWide(it->Name).c_str());
			}
		}

		it->InUse         = true;
		it->LastUsedFrame = m_frame;
		return it->Texture;
	}

	void TransientPool::Release(TextureHandle texture)
	{
		auto it = std::ranges::find(m_entries, texture, &Entry::Texture);
		assert(it != m_entries.end() && it->InUse && "Releasing a texture the pool did not hand out");
		it->InUse = false;
	}

	void TransientPool::EndFrame()
	{
		ZoneScoped;
		++m_frame;

		std::erase_if(m_entries, [this](const Entry& e)
		{
			const bool stale = !e.InUse && m_frame - e.LastUsedFrame > kEvictAfterFrames;
			if (stale)
			{
				KOI_LOG(RHI, Trace, "Transient texture evicted: '{}'", e.Name);
				m_device->Destroy(e.Texture);
			}
			return stale;
		});
	}
}
