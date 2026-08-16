#include "Koi/Engine/Logging/LogHistory.h"
#include "Koi/Engine/Memory/MemoryTracker.h"
#include <algorithm>

namespace koi
{
	LogHistory& LogHistory::Get()
	{
		static LogHistory instance;
		return instance;
	}

	void LogHistory::Resize(u32 capacity)
	{
		KOI_MEM_TAG_SCOPE(MemTag::Logging);
		std::lock_guard lock(m_mutex);

		m_slots.clear();
		m_slots.resize(capacity);
		m_written = 0;
	}

	void LogHistory::Clear()
	{
		std::lock_guard lock(m_mutex);

		// Keep the slot strings around so their capacity is reused after a clear
		for (LogRecord& record : m_slots)
		{
			record.Message.clear();
		}

		m_written = 0;
	}

	void LogHistory::Push(LogLevel level, u16 category, u32 line, f64 time, std::string_view message)
	{
		KOI_MEM_TAG_SCOPE(MemTag::Logging);
		std::lock_guard lock(m_mutex);

		if (m_slots.empty())
			return;

		LogRecord& record = m_slots[m_written % m_slots.size()];
		record.Time     = time;
		record.Line     = line;
		record.Category = category;
		record.Level    = level;
		record.Message.assign(message);  // reuses the recycled slot's buffer

		++m_written;
	}

	u64 LogHistory::ReadFrom(u64 from, std::vector<LogRecord>& out) const
	{
		KOI_MEM_TAG_SCOPE(MemTag::Logging);
		std::lock_guard lock(m_mutex);

		if (m_slots.empty()) 
			return m_written;

		const u64 capacity = m_slots.size();
		const u64 oldest   = (m_written > capacity) ? m_written - capacity : 0;

		for (u64 seq = std::max(from, oldest); seq < m_written; ++seq)
		{
			out.push_back(m_slots[seq % capacity]);
		}

		return m_written;
	}

	u64 LogHistory::Sequence() const
	{
		std::lock_guard lock(m_mutex);
		return m_written;
	}

	u32 LogHistory::Capacity() const
	{
		std::lock_guard lock(m_mutex);
		return static_cast<u32>(m_slots.size());
	}
}
