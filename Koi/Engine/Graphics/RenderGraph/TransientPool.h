#pragma once
#include "Koi/Engine/Graphics/Types.h"
#include <string>
#include <vector>

namespace koi
{
	class GfxDevice;

	// Render-graph textures, recycled by desc across frames
	class TransientPool : NonCopyable
	{
	public:
		void Init(GfxDevice& device);
		void Shutdown();

		[[nodiscard]] TextureHandle Acquire(const TextureDesc& desc);
		void Release(TextureHandle texture);

		// Destroys textures nothing has acquired for a while
		void EndFrame();

		[[nodiscard]] inline size_t GetTextureCount() const noexcept { return m_entries.size(); }

	private:
		[[nodiscard]] static bool Matches(const TextureDesc& a, const TextureDesc& b) noexcept;

	private:
		struct Entry
		{
			TextureDesc   Desc;  // DebugName cleared, it would dangle
			std::string   Name;
			TextureHandle Texture;
			u64           LastUsedFrame = 0;
			bool          InUse         = false;
		};

		GfxDevice*         m_device = nullptr;
		std::vector<Entry> m_entries;
		u64                m_frame  = 0;
	};
}
