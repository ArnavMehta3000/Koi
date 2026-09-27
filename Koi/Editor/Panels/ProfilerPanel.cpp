#include "Koi/Editor/Panels/ProfilerPanel.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <algorithm>
#include <cmath>

namespace koi::editor
{
	namespace
	{
		constexpr int kLegendWidth   = 260;
		constexpr f32 kAutoHeadroom   = 1.25f;
		constexpr f32 kAutoPercentile = 0.95f;  // spikes above it clip instead of squashing the graph
		constexpr f32 kAutoSmoothSecs = 0.25f;
	}

	u32 ProfilerPanel::ColorFor(std::string_view name)
	{
		if (auto it = m_colors.find(name); it != m_colors.end())
			return it->second;

		// Golden-ratio hue steps keep every new name far from all the ones before it
		const u32 n = u32(m_colors.size());
		const f32 hue = std::fmod(0.1f + f32(n) * 0.618034f, 1.0f);
		const f32 sat = (n % 2) ? 0.55f : 0.75f;
		const f32 val = (n % 3 == 2) ? 0.75f : 0.92f;

		ImVec4 rgb{ 0.0f, 0.0f, 0.0f, 1.0f };
		ImGui::ColorConvertHSVtoRGB(hue, sat, val, rgb.x, rgb.y, rgb.z);
		const u32 color = ImGui::ColorConvertFloat4ToU32(rgb);
		m_colors.emplace(name, color);
		return color;
	}

	void ProfilerPanel::OnDraw()
	{
		if (!m_paused)
		{
			const f32 dt = Engine::Get().GetStats().DeltaTime;
			Load(m_cpu, FrameStats::GetLastFrame(), dt);
			Load(m_gpu, Engine::Get().GetSubsystem<Renderer>().GetDevice().GetGpuTimers().GetLastFrame(), dt);
			m_frameOffset = 0;
		}

		if (ImGui::Begin("Profiler"))
		{
			const Engine::Stats stats = Engine::Get().GetStats();
			ImGui::Text("%u fps  %.2f ms  |  tracked cpu %.2f ms  gpu %.2f ms", stats.FPS, stats.DeltaTime * 1000.0f,
				m_cpu.Graph.GetTotalTaskTime(m_frameOffset) * 1000.0f, m_gpu.Graph.GetTotalTaskTime(m_frameOffset) * 1000.0f);

			ImGui::Checkbox("Pause", &m_paused);
			ImGui::SameLine();
			ImGui::Checkbox("Auto scale", &m_autoScale);
			if (!m_autoScale)
			{
				ImGui::SameLine();
				ImGui::SetNextItemWidth(120.0f);
				ImGui::SliderFloat("Scale", &m_scaleMs, 0.1f, 50.0f, "%.2f ms", ImGuiSliderFlags_Logarithmic);
			}
			if (m_paused)
			{
				ImGui::SameLine();
				ImGui::SetNextItemWidth(160.0f);
				ImGui::SliderInt("Frames back", &m_frameOffset, 0, int(kHistoryFrames) - 1);
			}

			const f32 labelHeight = ImGui::GetTextLineHeightWithSpacing();
			const f32 spacing     = ImGui::GetStyle().ItemSpacing.y;
			const int height      = std::max(int((ImGui::GetContentRegionAvail().y - 2.0f * labelHeight - spacing) * 0.5f), 40);
			DrawTrack(m_cpu, "CPU", height);
			DrawTrack(m_gpu, "GPU", height);
		}
		ImGui::End();
	}

	void ProfilerPanel::DrawTrack(Track& track, const char* label, int height)
	{
		const f32 scaleMs = m_autoScale ? std::max(track.ScaleMs, 0.05f) : m_scaleMs;
		ImGui::TextDisabled("%s  (%.2f ms)", label, scaleMs);

		const int width = std::max(int(ImGui::GetContentRegionAvail().x) - kLegendWidth, 1);
		track.Graph.RenderTimings(width, kLegendWidth, height, m_frameOffset, scaleMs / 1000.0f);
	}

	void ProfilerPanel::Load(Track& track, std::span<const FrameStatSample> samples, f32 dt)
	{
		track.Tasks.resize(samples.size());
		for (size_t i = 0; i < samples.size(); i++)
		{
			legit::ProfilerTask& task = track.Tasks[i];
			task.startTime = samples[i].Start;
			task.endTime   = samples[i].End;
			task.name.assign(samples[i].Name);
			task.color     = ColorFor(samples[i].Name);
		}
		track.Graph.LoadFrameData(track.Tasks.data(), track.Tasks.size());

		if (samples.empty()) return;

		track.SpansMs[track.SpanHead] = f32(samples.back().End) * 1000.0f;
		track.SpanHead  = (track.SpanHead + 1) % kHistoryFrames;
		track.SpanCount = std::min(track.SpanCount + 1, kHistoryFrames);

		std::array<f32, kHistoryFrames> sorted = track.SpansMs;
		const auto end = sorted.begin() + ptrdiff_t(track.SpanCount);
		const auto nth = sorted.begin() + ptrdiff_t(f32(track.SpanCount - 1) * kAutoPercentile);
		std::nth_element(sorted.begin(), nth, end);

		const f32 target = *nth * kAutoHeadroom;
		track.ScaleMs += (target - track.ScaleMs) * (1.0f - std::exp(-dt / kAutoSmoothSecs));
	}
}
