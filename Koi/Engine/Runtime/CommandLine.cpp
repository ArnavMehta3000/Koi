#include "Koi/Engine/Runtime/CommandLine.h"

namespace koi::internal
{
	static CommandLineFactory s_factory = [] { return std::make_unique<CommandLine>(); };

	void RegisterCommandLineFactory(CommandLineFactory factory)
	{
		s_factory = std::move(factory);
	}

	std::unique_ptr<CommandLine> InvokeCommandLineFactory()
	{
		return s_factory();
	}
}
