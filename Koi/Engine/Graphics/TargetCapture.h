#pragma once
#include "Koi/Engine/RHI/RenderGraph/RenderGraph.h"
#include <optional>
#include <string>

namespace koi
{
	enum class CaptureMode : u32 { Color, LinearDepth };

	struct CaptureSettings
	{
		CaptureMode Mode     = CaptureMode::Color;
		u32         Channels = 0b0111;  // bit per RGBA channel
		f32         RangeMin = 0.0f;
		f32         RangeMax = 1.0f;
	};

	// Copy of a texture from RG for preview purposes
	class TargetCapture : public IRGObserver, NonCopyable
	{
	public:
		struct Target
		{
			std::string Name;
			u16         Version = 0;
		};

		void Init(GfxDevice& device);
		void Shutdown();

		inline void Watch(std::optional<Target> target) { m_target = std::move(target); }
		[[nodiscard]] inline const std::optional<Target>& GetWatched() const noexcept { return m_target; }
		[[nodiscard]] inline CaptureSettings& GetSettings() noexcept { return m_settings; }

		// Linear depth needs the planes of the camera that rendered it
		inline void SetDepthPlanes(f32 nearZ, f32 farZ) noexcept { m_near = nearZ; m_far = farZ; }

		// Invalid unless the watched target was captured this frame
		[[nodiscard]] TextureHandle GetPreview() const;

		void OnWrite(RenderGraph& graph, std::string_view resource, RGTexture written) override;

	private:
		void EnsurePreview(u32 width, u32 height);

	private:
		GfxDevice*            m_device = nullptr;
		std::optional<Target> m_target;
		CaptureSettings       m_settings;
		f32                   m_near          = 0.1f;
		f32                   m_far           = 1000.0f;
		TextureHandle         m_preview;
		u64                   m_capturedFrame = ~0ull;
	};
}
