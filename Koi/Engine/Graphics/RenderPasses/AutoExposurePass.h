#pragma once
#include "Koi/Engine/Content/Shaders/Interop/PostProcess.hlsli"
#include "Koi/Engine/ECS/Components.h"
#include "Koi/Engine/Graphics/RenderFeature.h"
#include <array>

namespace koi
{
	class GfxDevice;

	class AutoExposurePass : NonCopyable
	{
	public:
		static constexpr std::string_view kName = "AutoExposure";

		struct Config
		{
			GfxDevice* Device = nullptr;
		};

		// Readback from GPU
		struct Stats
		{
			std::array<u32, HISTOGRAM_BIN_COUNT> Bins{};
			f32                  Exposure = 1.0f;
			AutoExposureSettings Settings;  // what the bins were built with
			bool                 Valid    = false;
		};

		explicit AutoExposurePass(const Config& config) : m_config(config) {}
		~AutoExposurePass();

		void Record(RenderGraph& graph, const RenderView& view, World& world);

		inline void RequestStats() noexcept { m_statsRequested = true; }
		[[nodiscard]] inline const Stats& GetStats() const noexcept { return m_stats; }

	private:
		void CollectStats();

	private:
		struct Readback
		{
			BufferHandle         Buffer;
			AutoExposureSettings Settings;
			bool                 Pending = false;
		};

		Config                                 m_config;
		std::array<Readback, FRAMES_IN_FLIGHT> m_readbacks{};
		Stats                                  m_stats;
		bool                                   m_statsRequested = false;
	};
}
