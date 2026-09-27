#include "Koi/Editor/Panels/ProfilerPanel.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace koi::editor
{
	namespace
	{
		constexpr int kLegendWidth   = 260;
		constexpr f32 kAutoHeadroom   = 1.25f;
		constexpr f32 kAutoPercentile = 0.95f;  // spikes above it clip instead of squashing the graph
		constexpr f32 kAutoSmoothSecs = 0.25f;

		constexpr std::array kBudgetFps    = { 0, 30, 60, 120, 144, 240 };
		constexpr const char* kBudgetItems = "Off\0" "30 fps\0" "60 fps\0" "120 fps\0" "144 fps\0" "240 fps\0";
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

			for (FrameCounter& counter : m_counters)
			{
				counter.Value = 0;
			}

			for (const FrameCounter& latest : FrameStats::GetLastCounters())
			{
				auto it = std::ranges::find(m_counters, latest.Name, &FrameCounter::Name);
				if (it == m_counters.end())
					m_counters.push_back(latest);
				else
					it->Value = latest.Value;
			}
		}

		if (ImGui::Begin("Profiler"))
		{
			const Engine::Stats stats = Engine::Get().GetStats();
			ImGui::Text("%u fps  %.2f ms  |  tracked cpu %.2f ms  gpu %.2f ms", stats.FPS, stats.DeltaTime * 1000.0f,
				m_cpu.Graph.GetTotalTaskTime(m_frameOffset) * 1000.0f, m_gpu.Graph.GetTotalTaskTime(m_frameOffset) * 1000.0f);

			ImGui::Checkbox("Pause", &m_paused);
			ImGui::SameLine();
			ImGui::Checkbox("Auto scale", &m_autoScale);
			ImGui::SameLine();
			ImGui::SetNextItemWidth(90.0f);
			ImGui::Combo("Budget", &m_budgetIndex, kBudgetItems);
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

			DrawCounters();

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

		const int fps = kBudgetFps[size_t(m_budgetIndex)];
		track.Graph.budgetTime = fps > 0 ? 1.0f / f32(fps) : 0.0f;

		const int width = std::max(int(ImGui::GetContentRegionAvail().x) - kLegendWidth, 1);
		track.Graph.RenderTimings(width, kLegendWidth, height, m_frameOffset, scaleMs / 1000.0f);
	}

	void ProfilerPanel::DrawCounters()
	{
		const f32 right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
		const f32 gap   = ImGui::GetFontSize();

		for (size_t i = 0; i < m_counters.size(); i++)
		{
			char name[64];
			char value[32];
			std::snprintf(name,  sizeof(name),  "%.*s", int(m_counters[i].Name.size()), m_counters[i].Name.data());
			std::snprintf(value, sizeof(value), "%lld", static_cast<long long>(m_counters[i].Value));

			const ImGuiStyle& style = ImGui::GetStyle();
			const f32 width = ImGui::CalcTextSize(name).x + style.ItemInnerSpacing.x + ImGui::CalcTextSize(value).x;
			if (i > 0)
			{
				ImGui::SameLine(0.0f, gap);
				if (ImGui::GetCursorScreenPos().x + width > right)
					ImGui::NewLine();
			}

			ImGui::TextDisabled("%s", name);
			ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
			ImGui::TextUnformatted(value);
		}
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
