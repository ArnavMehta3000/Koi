#include "Koi/Engine/Runtime/CommandLine.h"
#include "Koi/Engine/Logging/Logger.h"

namespace koi
{
	int CommandLine::ParseArgs(int argc, char* argv[])
	{
		try
		{
			CLI::App::parse(argc, argv);
		}
		catch (const std::exception& e)
		{
			KOI_LOG(Engine, Error, "Command line: {}", e.what());
			return 1;
		}
		return 0;
	}
}

namespace koi::internal
{
	static CommandLineFactory& Factory()
	{
		static CommandLineFactory s_factory = [] { return std::make_unique<CommandLine>(); };
		return s_factory;
	}

	void RegisterCommandLineFactory(CommandLineFactory factory)
	{
		Factory() = std::move(factory);
	}

	std::unique_ptr<CommandLine> InvokeCommandLineFactory()
	{
		return Factory()();
	}
}
