module;
#include <spdlog/spdlog.h>
#include <span>
#include <array>
export module Koi.Memory.MemoryDebug;

import Koi.Core.Logger;
import Koi.Memory.MemoryTags;
import Koi.Memory.MemoryTracker;

export namespace koi
{
	struct HumanBytes { std::size_t Bytes; };

	void PrintMemSnapshot(std::span<const MemTagSnapshot> snap);
	void PrintMemStats();
	MemTagSnapshotArray DiffSnapshots(std::span<const MemTagSnapshot> before, std::span<const MemTagSnapshot> after);
}

template <>
struct fmt::formatter<koi::HumanBytes> : fmt::formatter<std::string_view>
{
	auto format(koi::HumanBytes hb, fmt::format_context& ctx) const
	{
		constexpr double KB = 1024.0;
		constexpr double MB = KB * 1024.0;
		constexpr double GB = MB * 1024.0;
		const double b = static_cast<double>(hb.Bytes);

		char buf[32];
		std::string_view sv;

		if      (b >= GB) sv = std::string_view{ buf, fmt::format_to_n(buf, sizeof(buf), "{:.2f} GB", b / GB).size };
		else if (b >= MB) sv = std::string_view{ buf, fmt::format_to_n(buf, sizeof(buf), "{:.2f} MB", b / MB).size };
		else if (b >= KB) sv = std::string_view{ buf, fmt::format_to_n(buf, sizeof(buf), "{:.2f} KB", b / KB).size };
		else              sv = std::string_view{ buf, fmt::format_to_n(buf, sizeof(buf), "{} B",     hb.Bytes).size };

		return fmt::formatter<std::string_view>::format(sv, ctx);
	}
};
