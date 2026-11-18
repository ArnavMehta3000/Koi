#pragma once
#include "Koi/Engine/ECS/Observer.h"

// Test component for persistent entities
struct Traveller
{
	koi::u32 Loads = 0;
};

class TravellerObserver final : public koi::Observer<koi::SceneLoaded, Traveller>
{
public:
	void On(const koi::SceneLoaded& event, koi::Entity e, Traveller& traveller) override;
};

void RegisterTravellerObserver();
