#pragma once
#include "Koi/Editor/Core/Panel.h"
#include "Koi/Engine/Diagnostics/FrameStats.h"
#include <KoiProfiler.h>
#include <vector>
#include <IconsFontAwesome6.h>

template <>
struct koi::prof::TaskTraits<koi::FrameStatSample>
{
	static koi::prof::ProfilerTask ToTask(const koi::FrameStatSample& sample) { return { sample.Start, sample.End, sample.Name }; }
};

namespace koi::editor
{
	class ProfilerPanel : public Panel
	{
	public:
		static constexpr PanelInfo kInfo{ .Name = "Profiler", .Icon = ICON_FA_GAUGE_HIGH, .WindowFlags = ImGuiWindowFlags_NoScrollbar };

		void OnUpdate() override;
		void OnDraw() override;

	private:
		prof::ProfilerWindow       m_window;
		std::vector<prof::Counter> m_counters;  // reused staging for LoadCounters
	};
}
