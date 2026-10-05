#pragma once
#include "Koi/Engine/Diagnostics/FrameStats.h"
#include "Koi/Engine/Interop/EditorPanel.h"
#include <KoiProfiler.h>
#include <string>
#include <vector>

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
		void OnDraw() override;

	private:
		prof::ProfilerWindow       m_window;
		std::vector<prof::Counter> m_counters;  // reused staging for LoadCounters
	};
}
