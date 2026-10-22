#pragma once
#include "Koi/Editor/Core/EditorPanel.h"
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
	class ProfilerPanel : public IEditorPanel
	{
	public:
		[[nodiscard]] inline std::string_view GetName() const override { return "Profiler"; }
		[[nodiscard]] const char* GetIcon() const override { return ICON_FA_GAUGE_HIGH; }
		[[nodiscard]] ImGuiWindowFlags GetWindowFlags() const override { return ImGuiWindowFlags_NoScrollbar; }
		void OnUpdate() override;
		void OnDraw() override;

	private:
		prof::ProfilerWindow       m_window;
		std::vector<prof::Counter> m_counters;  // reused staging for LoadCounters
	};
}
