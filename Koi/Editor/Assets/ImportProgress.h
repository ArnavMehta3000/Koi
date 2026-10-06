#pragma once
#include "Koi/Engine/Core/Types.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <format>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace koi::editor
{
	// Written by an import worker, read by the UI every frame
	class ImportProgress : public NonCopyable
	{
	public:
		ImportProgress() = default;

		void Report(std::string_view stage, f32 fraction)
		{
			{
				std::scoped_lock lock(m_mutex);
				m_stage = m_prefix;
				m_stage += stage;
			}
			SetFraction(fraction);
		}

		void SetFraction(f32 fraction)
		{
			m_fraction.store(m_rangeBegin + fraction * (m_rangeEnd - m_rangeBegin), std::memory_order_relaxed);
		}

		// Maps what follows into [begin, end] of the bar, with a label in front. For one step of a bigger import
		void SetRange(f32 begin, f32 end, std::string_view prefix = {})
		{
			std::scoped_lock lock(m_mutex);
			m_rangeBegin = begin;
			m_rangeEnd   = end;
			m_prefix     = prefix;
		}

		[[nodiscard]] f32 Fraction() const { return m_fraction.load(std::memory_order_relaxed); }

		[[nodiscard]] std::string Stage() const
		{
			std::scoped_lock lock(m_mutex);
			return m_stage;
		}

		// Starts timing a named stage, closing the one before. A finished import can then say where its time went
		void BeginStage(std::string_view name)
		{
			const Clock::time_point now = Clock::now();
			std::scoped_lock lock(m_mutex);
			CloseStage(now);
			m_currentStage = name;
			m_stageStart   = now;
		}

		void EndStage()
		{
			const Clock::time_point now = Clock::now();
			std::scoped_lock lock(m_mutex);
			CloseStage(now);
		}

		// "decode 0.05 s, mips 0.12 s, compress 2.21 s", stages that ran more than once summed up
		[[nodiscard]] std::string Breakdown() const
		{
			std::scoped_lock lock(m_mutex);
			std::vector<std::pair<std::string_view, f64>> totals;
			for (const auto& [name, seconds] : m_timings)
			{
				auto it = std::ranges::find_if(totals, [&](const auto& total) { return total.first == name; });
				if (it == totals.end()) totals.emplace_back(name, seconds);
				else it->second += seconds;
			}

			std::string text;
			for (const auto& [name, seconds] : totals)
			{
				text += std::format("{}{} {:.2f} s", text.empty() ? "" : ", ", name, seconds);
			}
			return text;
		}

		void RequestCancel()                   { m_cancel.store(true, std::memory_order_relaxed); }
		[[nodiscard]] bool IsCancelled() const { return m_cancel.load(std::memory_order_relaxed); }

	private:
		using Clock = std::chrono::steady_clock;

		void CloseStage(Clock::time_point now)
		{
			if (m_currentStage.empty()) return;
			m_timings.emplace_back(std::move(m_currentStage), std::chrono::duration<f64>(now - m_stageStart).count());
			m_currentStage.clear();
		}

	private:
		std::atomic<f32>   m_fraction{ 0.0f };
		std::atomic<bool>  m_cancel{ false };
		mutable std::mutex m_mutex;
		std::string        m_stage = "Waiting";
		std::string        m_prefix;
		f32                m_rangeBegin = 0.0f;  // only touched by the worker
		f32                m_rangeEnd   = 1.0f;

		std::string                              m_currentStage;
		Clock::time_point                        m_stageStart{};
		std::vector<std::pair<std::string, f64>> m_timings;
	};
}
