#pragma once
#include <CLI/CLI.hpp>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace koi
{
	namespace fs = std::filesystem;

	class CommandLine : private CLI::App
	{
	public:
		CommandLine(const std::string& desc = "Koi Engine", const std::string& name = "")
			: CLI::App(desc, name)
		{
			add_option("-p ,--project", m_projectFile  , "Project file (overrides the launcher default)");
			add_flag("--d3ddebug"     , m_d3dDebug     , "Enable the D3D12 debug layer");
			add_flag("--gpuvalidation", m_gpuValidation, "Enable D3D12 GPU-based validation (implies --d3ddebug)");
			add_option("--log"        , m_logLevels    , "Category log levels, e.g. RHI=Trace,Assets=Debug; all=<level> sets every category")->delimiter(',');
		}

		virtual ~CommandLine() = default;

		int ParseArgs(int argc, char* argv[]);

		// Used by RunApplication when -p is absent
		void OverrideProjectFile(fs::path p) { m_projectFile = std::move(p); }

		const fs::path& GetProjectFile()               const noexcept { return m_projectFile;                 }
		bool IsD3DDebugEnabled()                       const noexcept { return m_d3dDebug || m_gpuValidation; }
		bool IsGpuValidationEnabled()                  const noexcept { return m_gpuValidation;               }
		const std::vector<std::string>& GetLogLevels() const noexcept { return m_logLevels;                   }

	private:
		fs::path                 m_projectFile;
		bool                     m_d3dDebug      = false;
		bool                     m_gpuValidation = false;
		std::vector<std::string> m_logLevels;
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
