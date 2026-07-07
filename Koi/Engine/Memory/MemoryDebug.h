#pragma once
#include "Koi/Engine/Memory/MemoryTracker.h"
#include "Koi/Engine/Logging/Logger.h"
#include <span>

namespace koi
{
	struct HumanBytes { std::size_t Bytes; };
}

template <>
struct fmt::formatter<koi::HumanBytes> : fmt::formatter<std::string_view>
{
	auto format(koi::HumanBytes hb, format_context& ctx) const {
		constexpr double KB = 1024.0;
		constexpr double MB = KB * 1024.0;
		constexpr double GB = MB * 1024.0;
		const double b = static_cast<double>(hb.Bytes);

		char buf[32];
		std::string_view sv;

		if (b >= GB)      sv = std::string_view{ buf, fmt::format_to_n(buf, sizeof(buf), "{:.2f} GB", b / GB).size };
		else if (b >= MB) sv = std::string_view{ buf, fmt::format_to_n(buf, sizeof(buf), "{:.2f} MB", b / MB).size };
		else if (b >= KB) sv = std::string_view{ buf, fmt::format_to_n(buf, sizeof(buf), "{:.2f} KB", b / KB).size };
		else              sv = std::string_view{ buf, fmt::format_to_n(buf, sizeof(buf), "{} B",     hb.Bytes).size };

		return fmt::formatter<std::string_view>::format(sv, ctx);
	}
};

namespace koi
{
	inline void PrintMemSnapshot(std::span<const MemTagSnapshot> snap)
	{
		KOI_LOG_INFO("===== Memory Stats =====");
		KOI_LOG_INFO("{:<12} {:>12} {:>12} {:>10} {:>10} {:>10}",
			"Tag", "Current", "Peak", "Allocs", "Frees", "Live");

		std::size_t totalCurrent = 0, totalPeak = 0;
		std::size_t totalAllocs = 0, totalFrees = 0;

		for (const auto& e : snap)
		{
			if (e.Allocs== 0 && e.Current == 0)
				continue;

			KOI_LOG_INFO("{:<12} {:>12} {:>12} {:>10} {:>10} {:>10}",
				ToString(e.Tag),
				HumanBytes{ e.Current },
				HumanBytes{ e.Peak },
				e.Allocs,
				e.Frees,
				e.Allocs - e.Frees);

			totalCurrent += e.Current;
			totalPeak    += e.Peak;
			totalAllocs  += e.Allocs;
			totalFrees   += e.Frees;
		}

		KOI_LOG_INFO("{:-<70}", "");
		KOI_LOG_INFO("{:<12} {:>12} {:>12} {:>10} {:>10} {:>10}",
			"TOTAL",
			HumanBytes{ totalCurrent },
			HumanBytes{ totalPeak },
			totalAllocs,
			totalFrees,
			totalAllocs - totalFrees);
	}

	inline void PrintMemStats()
	{
		const auto snapshot = GetMemSnapshot();
		PrintMemSnapshot(snapshot);
	}

	inline auto DiffSnapshots(std::span<const MemTagSnapshot> before, std::span<const MemTagSnapshot> after)
	{
		MemTagSnapshotArray out{};

		for (std::size_t i = 0; i < out.size(); ++i)
		{
			// current/peak as signed deltas would need a different type;
			// here we keep the "after" values for current/peak and diff the counters.
			out[i].Tag     = after[i].Tag;
			out[i].Current = after[i].Current;          // absolute
			out[i].Peak    = after[i].Peak;             // absolute
			out[i].Allocs  = after[i].Allocs - before[i].Allocs;
			out[i].Frees   = after[i].Frees - before[i].Frees;
		}
		return out;
	}
}
