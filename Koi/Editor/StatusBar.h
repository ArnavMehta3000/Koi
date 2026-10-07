#pragma once
#include "Koi/Engine/Core/Types.h"
#include <string_view>

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
		bool             Importing = false;
	};

	// Bottom bar of the main window; fills with the play colour while the game runs
	class StatusBar
	{
	public:
		void Draw(const StatusInfo& info);
	};
}
