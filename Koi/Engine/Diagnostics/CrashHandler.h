#pragma once
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

		[[nodiscard]] static std::string Format(const std::stacktrace& trace);
	};
}
