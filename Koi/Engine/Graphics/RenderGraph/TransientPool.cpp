#include "Koi/Engine/Graphics/RenderGraph/TransientPool.h"
#include "Koi/Engine/Core/Assert.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Text/StringUtils.h"
#include <algorithm>
#include <type_traits>

namespace koi
{
	namespace
	{
		constexpr u64 kEvictAfterFrames = 30;  // Long enough to ride out a window drag without recreating every frame

		bool Matches(const TextureDesc& a, const TextureDesc& b) noexcept
		{
			return a.Width        == b.Width
				&& a.Height       == b.Height
				&& a.DepthOrArray == b.DepthOrArray
				&& a.MipLevels    == b.MipLevels
				&& a.SampleCount  == b.SampleCount
				&& a.Format       == b.Format
				&& a.Usage        == b.Usage;
		}

		bool Matches(const BufferDesc& a, const BufferDesc& b) noexcept
		{
			return a.Size   == b.Size
				&& a.Stride == b.Stride
				&& a.Usage  == b.Usage
				&& a.Memory == b.Memory;
		}

		TextureHandle Create(GfxDevice& device, const TextureDesc& desc) { return device.CreateTexture(desc); }
		BufferHandle  Create(GfxDevice& device, const BufferDesc& desc)  { return device.CreateBuffer(desc);  }
	}

	void TransientPool::Init(GfxDevice& device)
	{
		m_device = &device;
	}

	void TransientPool::Shutdown()
	{
		if (!m_device) return;

		for (const TextureEntry& e : m_textures)
		{
			m_device->Destroy(e.Resource);
		}
		for (const BufferEntry& e : m_buffers)
		{
			m_device->Destroy(e.Resource);
		}
		m_textures.clear();
		m_buffers.clear();
	}

	template <typename E, typename D>
	auto TransientPool::AcquireFrom(std::vector<E>& entries, const D& desc)
	{
		ZoneScoped;
		KOI_ASSERT(m_device, "TransientPool used before Init");

		auto it = std::ranges::find_if(entries, [&](const E& e) { return !e.InUse && Matches(e.Desc, desc); });
		if (it == entries.end())
		{
			E& e = entries.emplace_back(E
			{
				.Desc     = desc,
				.Name     = std::string(desc.DebugName),
				.Resource = Create(*m_device, desc),
			});
			e.Desc.DebugName = {};

			if constexpr (std::same_as<D, TextureDesc>)
			{
				KOI_LOG(RHI, Trace, "Transient texture created: '{}' {}x{}", e.Name, desc.Width, desc.Height);
			}
			else
			{
				KOI_LOG(RHI, Trace, "Transient buffer created: '{}' {} bytes", e.Name, desc.Size);
			}
			it = std::prev(entries.end());
		}
		else if (it->Name != desc.DebugName)
		{
			// Recycled under another name, rename it
			it->Name = std::string(desc.DebugName);
			if (auto* resource = m_device->Get(it->Resource))
			{
				resource->Resource->SetName(utils::ToWide(it->Name).c_str());
			}
		}

		it->InUse         = true;
		it->LastUsedFrame = m_frame;
		return it->Resource;
	}

	template <typename E, typename H>
	void TransientPool::ReleaseFrom(std::vector<E>& entries, H handle)
	{
		auto it = std::ranges::find(entries, handle, &E::Resource);
		KOI_ASSERT(it != entries.end() && it->InUse, "Releasing a resource the pool did not hand out");
		it->InUse = false;
	}

	template <typename E>
	void TransientPool::Evict(std::vector<E>& entries)
	{
		std::erase_if(entries, [this](const E& e)
		{
			const bool stale = !e.InUse && m_frame - e.LastUsedFrame > kEvictAfterFrames;
			if (stale)
			{
				KOI_LOG(RHI, Trace, "Transient resource evicted: '{}'", e.Name);
				m_device->Destroy(e.Resource);
			}
			return stale;
		});
	}

	TextureHandle TransientPool::Acquire(const TextureDesc& desc) { return AcquireFrom(m_textures, desc); }
	BufferHandle  TransientPool::Acquire(const BufferDesc& desc)  { return AcquireFrom(m_buffers, desc);  }
	void TransientPool::Release(TextureHandle texture)            { ReleaseFrom(m_textures, texture); }
	void TransientPool::Release(BufferHandle buffer)              { ReleaseFrom(m_buffers, buffer);   }

	void TransientPool::EndFrame()
	{
		ZoneScoped;
		++m_frame;
		Evict(m_textures);
		Evict(m_buffers);
	}
}
