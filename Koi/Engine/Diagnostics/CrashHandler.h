#pragma once
#include <exception>
#include <filesystem>
#include <stacktrace>
#include <string>

namespace koi
{
	namespace fs = std::filesystem;

	class CrashHandler
	{
	public:
		static void Install();

		// Nothing dumped if not set
		static void SetDumpDir(fs::path dir);

		// The stack at the most recent throw on this thread, captured before unwinding
		[[nodiscard]] static const std::stacktrace& LastThrowTrace();

		// The trace a koi::Exception carries, else LastThrowTrace
		[[nodiscard]] static const std::stacktrace& TraceOf(const std::exception& e);

		[[nodiscard]] static std::string Format(const std::stacktrace& trace);
	};
}
