#include "Platform/Application.h"

int main()
{
	koi::Application app;

	auto& mainWin = app.CreateWindow({ .Title = L"Main Viewport", .Width = 1800, .Height = 1000});
	auto& auxWin = app.CreateWindow({ .Title = L"Debug View",   .Width = 800,  .Height = 600 });
	
	while (app.Tick())
	{
		// Updating...
	}
	
	return 0;
}
