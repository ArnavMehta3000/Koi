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
		// Process-wide: records a stack for every C++ throw, and logs + dumps crashes and std::terminate
		static void Install();

		// Minidumps go here; nothing is written until it's set
		static void SetDumpDir(fs::path dir);

		// The stack at the most recent throw on this thread, captured before unwinding
		[[nodiscard]] static const std::stacktrace& LastThrowTrace();

		[[nodiscard]] static std::string Format(const std::stacktrace& trace);
	};
}
