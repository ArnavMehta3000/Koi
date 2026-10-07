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
			add_option("-p,--project", m_projectFile, "Project file (overrides the launcher default)");
			add_flag("--d3ddebug", m_d3dDebug, "Enable the D3D12 debug layer");
			add_flag("--gpuvalidation", m_gpuValidation, "Enable D3D12 GPU-based validation (implies --d3ddebug)");
		}

		virtual ~CommandLine() = default;

		int ParseArgs(int argc, char* argv[]);

		inline const fs::path& GetProjectFile() const { return m_projectFile; }

		// Used by RunApplication when -p is absent
		inline void OverrideProjectFile(fs::path p) { m_projectFile = std::move(p); }

		inline bool IsD3DDebugEnabled()      const { return m_d3dDebug || m_gpuValidation; }
		inline bool IsGpuValidationEnabled() const { return m_gpuValidation; }

	private:
		fs::path m_projectFile;
		bool     m_d3dDebug      = false;
		bool     m_gpuValidation = false;
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
