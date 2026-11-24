#include "Koi/Engine/Core/Assert.h"
#include "Koi/Engine/Diagnostics/CrashHandler.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Core/Win32.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>

namespace koi::internal
{
	void OnAssertFailed(std::string_view condition, std::string_view message, const std::source_location& where)
	{
		const std::string text = std::format("Assertion failed: {}{}{} at {}:{}\n{}",
			condition, message.empty() ? "" : " - ", message,
			fs::path(where.file_name()).filename().string(), where.line(),
			CrashHandler::Format(std::stacktrace::current(1)));

		if (Logger::IsInitialized())
		{
			KOI_LOG(Engine, Critical, "{}", text);
			Logger::Flush();
		}
		else
		{
			std::fputs(text.c_str(), stderr);
		}

		if (::IsDebuggerPresent())
		{
			__debugbreak();
			return;
		}
		std::abort();
	}
}
