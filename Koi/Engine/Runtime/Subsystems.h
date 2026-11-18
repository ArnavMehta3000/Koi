#pragma once
#include "Koi/Engine/Platform/InputActions.h"

namespace koi
{
	class Window;

	// Handed to every subsystem's Init, once the main window exists
	struct SubsystemInitContext
	{
		Window*            MainWindow = nullptr;
		bool               VSync = false;
		const InputConfig& Input;
	};
}
