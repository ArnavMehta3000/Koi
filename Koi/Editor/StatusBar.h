#pragma once
#include "Koi/Engine/Core/Types.h"
#include "Koi/Engine/Logging/LogHistory.h"
#include <string>
#include <string_view>
#include <vector>

namespace koi
{
	enum class PlayState : u8;
}

namespace koi::editor
{
	struct StatusInfo
	{
		PlayState        State;
		std::string_view ScenePath;
		std::string_view Message;    // last Save/Cook result
		bool             Importing = false;
	};

	// Bottom bar of the main window; fills with the play colour while the game runs
	class StatusBar
	{
	public:
		void Draw(const StatusInfo& info);

	private:
		void PullLog();

	private:
		std::vector<LogRecord> m_scratch;
		u64                    m_logCursor = 0;
		u32                    m_warnings  = 0;
		u32                    m_errors    = 0;
		std::string            m_lastWarning;
		std::string            m_lastError;
	};
}
