#include "Core/Logger.h"

int main()
{
	koi::Logger::Init();
	koi::Logger::Shutdown();

	return 0;
}
