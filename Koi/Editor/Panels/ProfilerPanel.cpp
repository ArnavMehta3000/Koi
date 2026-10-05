#include "Koi/Editor/Panels/ProfilerPanel.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Runtime/Engine.h"

namespace koi::editor
{
	void ProfilerPanel::OnDraw()
	{
		m_window.LoadFrameData(0, FrameStats::GetLastFrame());
		m_window.LoadFrameData(1, Engine::Get().GetSubsystem<Renderer>().GetDevice().GetGpuTimers().GetLastFrame());

		m_counters.clear();
		for (const FrameCounter& counter : FrameStats::GetLastCounters())
		{
			m_counters.push_back(prof::MakeCounter(counter.Name, counter.Value));
		}
		m_window.LoadCounters(m_counters.data(), m_counters.size());

		if (ImGui::Begin("Profiler", nullptr, ImGuiWindowFlags_NoScrollbar))
		{
			m_window.RenderContents();
		}
		ImGui::End();
	}
}
