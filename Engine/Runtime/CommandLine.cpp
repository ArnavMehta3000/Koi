module;
#include "Platform/Win32.h"
#include <CLI/CLI.hpp>
#include <filesystem>
#include <functional>
#include <memory>
#include <format>
module Koi.Runtime.CommandLine;

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
			OutputDebugStringA(std::format("CLI Parse Error: {}\n", e.what()).c_str());
			return 1;
		}
		return 0;
	}

	namespace internal
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
}
