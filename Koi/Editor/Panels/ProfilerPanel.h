#pragma once
#include "Koi/Engine/Core/Types.h"
#include "Koi/Engine/Diagnostics/FrameStats.h"
#include "Koi/Engine/Interop/EditorPanel.h"
#include <ImGuiProfilerRenderer.h>
#include <array>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace koi::editor
{
	class ProfilerPanel : public IEditorPanel
	{
	public:
		[[nodiscard]] inline std::string_view GetName() const override { return "Profiler"; }
		void OnDraw() override;

	private:
		static constexpr size_t kHistoryFrames = 300;

		struct Track
		{
			ImGuiUtils::ProfilerGraph        Graph{ kHistoryFrames };
			std::vector<legit::ProfilerTask> Tasks;  // reused staging for the graph
			std::array<f32, kHistoryFrames>  SpansMs{};
			size_t                           SpanHead  = 0;
			size_t                           SpanCount = 0;
			f32                              ScaleMs  = 1000.0f / 60.0f;
		};

		void Load(Track& track, std::span<const FrameStatSample> samples, f32 dt);
		void DrawTrack(Track& track, const char* label, int height);
		void DrawCounters();
		[[nodiscard]] u32 ColorFor(std::string_view name);

	private:
		Track m_cpu;
		Track m_gpu;
		std::map<std::string, u32, std::less<>> m_colors;
		std::vector<FrameCounter>               m_counters;  // every counter seen so far, zeroed when absent

		int  m_budgetIndex = 2;
		int  m_frameOffset = 0;
		f32  m_scaleMs     = 1000.0f / 60.0f;
		bool m_paused      = false;
		bool m_autoScale   = true;
	};
}
