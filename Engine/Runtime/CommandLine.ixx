module;
#include <CLI/CLI.hpp>
#include <filesystem>
#include <functional>
#include <memory>
export module Koi.Runtime.CommandLine;

export namespace koi
{
	namespace fs = std::filesystem;

	class CommandLine : private CLI::App
	{
	public:
		CommandLine(const std::string& desc = "Koi Engine", const std::string& name = "")
			: CLI::App(desc, name)
		{
			add_option("-p,--project",        m_projectFile,   "Project file");
			add_option("--engine-source",     m_engineSource,  "Engine source-tree root (editor only)");
			add_option("--project-source",    m_projectSource, "Project source-tree root (editor only)");
		}

		virtual ~CommandLine() = default;

		int ParseArgs(int argc, char* argv[]);

		[[nodiscard]] const fs::path& GetProjectFile()   const { return m_projectFile;   }
		[[nodiscard]] const fs::path& GetEngineSource()  const { return m_engineSource;  }
		[[nodiscard]] const fs::path& GetProjectSource() const { return m_projectSource; }

		void OverrideProjectFile(fs::path p) { m_projectFile = std::move(p); }

	private:
		fs::path m_projectFile;
		fs::path m_engineSource;
		fs::path m_projectSource;
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
}
