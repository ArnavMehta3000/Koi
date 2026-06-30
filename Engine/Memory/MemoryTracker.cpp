module;
#include <cstddef>
module Koi.Memory.MemoryTracker;

namespace koi
{
	MemTagSnapshotArray GetMemSnapshot()
	{
		MemTagSnapshotArray out{};
		auto& t = MemoryTracker::Get();

		for (std::size_t i = 0; i < out.size(); ++i)
		{
			auto tag      = static_cast<MemTag>(i);
			const auto& s = t.GetStats(tag);
			out[i] = { tag, s.Current.load(), s.Peak.load(), s.AllocCount.load(), s.FreeCount.load() };
		}
		return out;
	}
}
