#pragma once
#include "Koi/Engine/Core/LogLevel.h"
#include "Koi/Engine/Core/Types.h"
#include <mutex>
#include <string>
#include <vector>

namespace koi
{
	struct LogRecord
	{
		f64         Time = 0.0;  // seconds since the logger was configured
		u32         Line = 0;
		u16         Category = 0;  // Logger category id
		LogLevel    Level = LogLevel::Info;
		std::string Message;
	};

	class LogHistory
	{
	public:
		static LogHistory& Get();

		// 0 disables history entirely. Drops whatever is currently held
		void Resize(u32 capacity);
		void Clear();

		void Push(LogLevel level, u16 category, u32 line, f64 time, std::string_view message);

		// Appends every record with sequence >= from (oldest first) and returns the new cursor
		// Records that aged out of the ring are skipped rather than reported
		[[nodiscard]] u64 ReadFrom(u64 from, std::vector<LogRecord>& out) const;

		[[nodiscard]] u64 Sequence() const;
		[[nodiscard]] u32 Capacity() const;

	private:
		LogHistory() = default;

		mutable std::mutex     m_mutex;
		std::vector<LogRecord> m_slots;        // sized to capacity, indexed by sequence % capacity
		u64                    m_written = 0;  // total pushes ever, i.e. the next sequence number
	};
}
