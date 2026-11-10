#include "Koi/Engine/ECS/Observer.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Runtime/Engine.h"

namespace koi::internal
{
	bool IsGamePlaying()
	{
		return Engine::Get().IsPlaying();
	}

	void ReportEditObserverFailure(std::string_view observer, std::string_view what)
	{
		KOI_LOG(ECS, Error, "Observer '{}' threw: {}", observer, what);
	}
}
