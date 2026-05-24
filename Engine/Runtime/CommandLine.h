#pragma once
#include <CLI/CLI.hpp>
#include <filesystem>
#include <functional>
#include <memory>

namespace koi
{
	namespace fs = std::filesystem;

	class CommandLine : private CLI::App
	{
	public:
		CommandLine(const std::string& desc = "Koi Engine", const std::string& name = "")
			: CLI::App(desc, name)
		{
			add_option("-p,--project", m_projectFile, "Project file");
		}

		virtual ~CommandLine() = default;

		int ParseArgs(int argc, char* argv[])
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

		inline const fs::path& GetProjectFile() const { return m_projectFile; }

	private:
		fs::path m_projectFile;
	};

	namespace internal
	{
		using CommandLineFactory = std::function<std::unique_ptr<CommandLine>()>;
		void RegisterCommandLineFactory(CommandLineFactory factory);
		std::unique_ptr<CommandLine> InvokeCommandLineFactory();
	}

	template<typename T>
	struct CommandLineRegistrar
	{
		CommandLineRegistrar()
		{
			internal::RegisterCommandLineFactory([] { return std::make_unique<T>(); });
		}
	};

	// NOTE: Use static koi::CommandLineRegistrar<T> s_cmdReg to update the command line handler
}
