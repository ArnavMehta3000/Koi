module;
#include "Core/Config.h"
#include <array>
module Koi.Memory.MemoryDebug;

import Koi.Memory.MemoryTracker;
// import Koi.Core.Logger;
#include "Core/Types.h"
#include "Core/Logger.h"

namespace koi
{
	void PrintMemSnapshot(std::span<const MemTagSnapshot> snap)
	{
		KOI_LOG_INFO("===== Memory Stats =====");
		KOI_LOG_INFO("{:<12} {:>12} {:>12} {:>10} {:>10} {:>10}",
			"Tag", "Current", "Peak", "Allocs", "Frees", "Live");

		size_t totalCurrent = 0, totalPeak = 0;
		size_t totalAllocs = 0, totalFrees = 0;

		for (const auto& e : snap)
		{
			if (e.Allocs == 0 && e.Current == 0)
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

	void PrintMemStats()
	{
		const auto snapshot = GetMemSnapshot();
		PrintMemSnapshot(snapshot);
	}

	MemTagSnapshotArray DiffSnapshots(std::span<const MemTagSnapshot> before, std::span<const MemTagSnapshot> after)
	{
		MemTagSnapshotArray out{};
		for (std::size_t i = 0; i < out.size(); ++i)
		{
			out[i].Tag     = after[i].Tag;
			out[i].Current = after[i].Current;
			out[i].Peak    = after[i].Peak;
			out[i].Allocs  = after[i].Allocs - before[i].Allocs;
			out[i].Frees   = after[i].Frees  - before[i].Frees;
		}
		return out;
	}
}
