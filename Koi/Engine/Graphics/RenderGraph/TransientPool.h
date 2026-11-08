#pragma once
#include "Koi/Engine/Graphics/Types.h"
#include <string>
#include <vector>

namespace koi
{
	class GfxDevice;

	// Render-graph textures and buffers, recycled by desc across frames
	class TransientPool : NonCopyable
	{
	public:
		void Init(GfxDevice& device);
		void Shutdown();

		[[nodiscard]] TextureHandle Acquire(const TextureDesc& desc);
		[[nodiscard]] BufferHandle  Acquire(const BufferDesc& desc);
		void Release(TextureHandle texture);
		void Release(BufferHandle buffer);

		// Destroys resources nothing has acquired for a while
		void EndFrame();

		[[nodiscard]] inline size_t GetTextureCount() const noexcept { return m_textures.size(); }
		[[nodiscard]] inline size_t GetBufferCount()  const noexcept { return m_buffers.size();  }

	private:
		template <typename D, typename H>
		struct Entry
		{
			D           Desc;  // DebugName cleared, it would dangle
			std::string Name;
			H           Resource;
			u64         LastUsedFrame = 0;
			bool        InUse         = false;
		};

		using TextureEntry = Entry<TextureDesc, TextureHandle>;
		using BufferEntry  = Entry<BufferDesc, BufferHandle>;

		template <typename E, typename D>
		[[nodiscard]] auto AcquireFrom(std::vector<E>& entries, const D& desc);

		template <typename E, typename H>
		static void ReleaseFrom(std::vector<E>& entries, H handle);

		template <typename E>
		void Evict(std::vector<E>& entries);

	private:
		GfxDevice*                m_device = nullptr;
		std::vector<TextureEntry> m_textures;
		std::vector<BufferEntry>  m_buffers;
		u64                       m_frame  = 0;
	};
}
