#pragma once
#include <CLI/CLI.hpp>
#include <filesystem>

namespace koi
{
	namespace fs = std::filesystem;

	class CommandLine : private CLI::App
	{
	public:
		CommandLine() : CLI::App("Koi Engine")
		{
			add_option("-p,--project", m_projectFile, "Project file");
		}

		int ParseArgs(int argc, char* argv[])
		{
			try
			{
				CLI::App::parse(argc, argv);
			}
			catch (const std::exception& e)
			{
				return 1;
				throw e;
			}
			return 0;
		}

		inline const fs::path& GetProjectFile() const { return m_projectFile; }

	private:
		fs::path m_projectFile;
	};
}
