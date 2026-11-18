#include "Components/Traveller.h"
#include "Koi/Engine/ECS/ObserverRegistry.h"
#include "Koi/Engine/ECS/World.h"
#include "Koi/Engine/Logging/Logger.h"

KOI_LOG_CATEGORY(Traveller);

void TravellerObserver::On(const koi::SceneLoaded&, koi::Entity e, Traveller& traveller)
{
	++traveller.Loads;
	KOI_LOG(Traveller, Info, "'{}' survived into '{}' ({} loads)", e.GetName(), koi::World::From(e.GetECS()).GetSourcePath(), traveller.Loads);
}

void RegisterTravellerObserver()
{
	koi::ObserverRegistry::Get().Register<TravellerObserver>();
}
