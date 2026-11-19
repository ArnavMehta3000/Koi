#include "Koi/Engine/Diagnostics/CrashHandler.h"
#include "Koi/Engine/Core/Exception.h"
#include "Koi/Engine/Core/Types.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Core/Win32.h"
#include <DbgHelp.h>
#include <atomic>
#include <cstdlib>
#include <exception>
#include <format>
#include <vector>

namespace koi
{
	namespace
	{
		constexpr DWORD kCppExceptionCode = 0xE06D7363;

		thread_local std::stacktrace t_lastThrow;
		thread_local const void*     t_lastThrowObject = nullptr;
		thread_local bool            t_skipCapture     = false;

		fs::path         s_dumpDir;
		std::atomic_flag s_crashing;

		LONG CALLBACK OnException(EXCEPTION_POINTERS* info)
		{
			const EXCEPTION_RECORD& record = *info->ExceptionRecord;
			if (record.ExceptionCode != kCppExceptionCode || t_skipCapture)
				return EXCEPTION_CONTINUE_SEARCH;

			// `throw;` raises the same object again, so keep the original throw's stack
			const void* object = record.NumberParameters >= 2 ? reinterpret_cast<const void*>(record.ExceptionInformation[1]) : nullptr;
			if (object && object == t_lastThrowObject)
				return EXCEPTION_CONTINUE_SEARCH;

			t_skipCapture = true;
			try
			{
				t_lastThrow       = std::stacktrace::current();
				t_lastThrowObject = object;
			}
			catch (...) {}
			t_skipCapture = false;

			return EXCEPTION_CONTINUE_SEARCH;
		}

		std::string Describe(const EXCEPTION_RECORD& record)
		{
			switch (record.ExceptionCode)
			{
				case EXCEPTION_ACCESS_VIOLATION:
				{
					if (record.NumberParameters < 2) return "access violation";
					const ULONG_PTR op = record.ExceptionInformation[0];
					return std::format("access violation {} 0x{:016X}", op == 0 ? "reading" : op == 1 ? "writing" : "executing", record.ExceptionInformation[1]);
				}
				case EXCEPTION_STACK_OVERFLOW:      return "stack overflow";
				case EXCEPTION_INT_DIVIDE_BY_ZERO:  return "integer divide by zero";
				case EXCEPTION_ILLEGAL_INSTRUCTION: return "illegal instruction";
				case EXCEPTION_PRIV_INSTRUCTION:    return "privileged instruction";
				case EXCEPTION_IN_PAGE_ERROR:       return "in-page error";
				case kCppExceptionCode:             return "unhandled C++ exception";
				default:                            return std::format("exception 0x{:08X}", record.ExceptionCode);
			}
		}

		void WriteDump(EXCEPTION_POINTERS* info)
		{
			if (s_dumpDir.empty()) return;

			std::error_code ec;
			fs::create_directories(s_dumpDir, ec);

			SYSTEMTIME t;
			::GetLocalTime(&t);
			const fs::path path = s_dumpDir / std::format("Crash-{:04}{:02}{:02}-{:02}{:02}{:02}.dmp", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);

			const HANDLE file = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (file == INVALID_HANDLE_VALUE)
			{
				KOI_LOG(Engine, Error, "Couldn't create crash dump {}", path.string());
				return;
			}

			MINIDUMP_EXCEPTION_INFORMATION exception
			{
				.ThreadId          = ::GetCurrentThreadId(),
				.ExceptionPointers = info,
				.ClientPointers    = FALSE,
			};

			const auto type = static_cast<MINIDUMP_TYPE>(MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithThreadInfo);
			const BOOL ok = ::MiniDumpWriteDump(::GetCurrentProcess(), ::GetCurrentProcessId(), file, type, info ? &exception : nullptr, nullptr, nullptr);
			::CloseHandle(file);

			if (ok)
			{
				KOI_LOG(Engine, Critical, "Crash dump written to {}", path.string());
			}
			else
			{
				KOI_LOG(Engine, Error, "Couldn't write crash dump {} (0x{:08X})", path.string(), ::GetLastError());
			}
		}

		LONG WINAPI OnUnhandled(EXCEPTION_POINTERS* info)
		{
			if (s_crashing.test_and_set())
				return EXCEPTION_CONTINUE_SEARCH;

			const EXCEPTION_RECORD& record = *info->ExceptionRecord;

			// Symbolizing needs more stack than an overflow leaves
			std::string trace = "\t<skipped>";
			if (record.ExceptionCode == kCppExceptionCode)
				trace = CrashHandler::Format(t_lastThrow);
			else if (record.ExceptionCode != EXCEPTION_STACK_OVERFLOW)
				trace = CrashHandler::Format(std::stacktrace::current());

			KOI_LOG(Engine, Critical, "Crashed: {} at {}\n{}", Describe(record), record.ExceptionAddress, trace);
			WriteDump(info);
			Logger::Flush();

			return EXCEPTION_EXECUTE_HANDLER;
		}

		[[noreturn]] void OnTerminate()
		{
			if (!s_crashing.test_and_set())
			{
				std::string reason = "no active exception";
				std::stacktrace trace;

				if (const std::exception_ptr current = std::current_exception())
				{
					trace = t_lastThrow;
					t_skipCapture = true;
					try                               { std::rethrow_exception(current); }
					catch (const std::exception& e)   { reason = e.what(); }
					catch (...)                       { reason = "non-std exception"; }
					t_skipCapture = false;
				}
				else
				{
					trace = std::stacktrace::current();
				}

				KOI_LOG(Engine, Critical, "std::terminate: {}\n{}", reason, CrashHandler::Format(trace));
				WriteDump(nullptr);
				Logger::Flush();
			}

			if (::IsDebuggerPresent())
			{
				__debugbreak();
			}
			std::abort();
		}
	}

	void CrashHandler::Install()
	{
		static bool installed = false;
		if (installed) return;
		installed = true;

		::AddVectoredExceptionHandler(1, OnException);
		::SetUnhandledExceptionFilter(OnUnhandled);
		std::set_terminate(OnTerminate);

		// Leaves the main thread room to log a stack overflow
		ULONG reserve = 64 * 1024;
		::SetThreadStackGuarantee(&reserve);
	}

	void CrashHandler::SetDumpDir(fs::path dir)
	{
		s_dumpDir = std::move(dir);
	}

	const std::stacktrace& CrashHandler::LastThrowTrace()
	{
		return t_lastThrow;
	}

	const std::stacktrace& CrashHandler::TraceOf(const std::exception& e)
	{
		const auto* traced = dynamic_cast<const Exception*>(&e);
		return traced && !traced->GetTrace().empty() ? traced->GetTrace() : t_lastThrow;
	}

	std::string CrashHandler::Format(const std::stacktrace& trace)
	{
		if (trace.empty()) return "\t<no stack trace>";

		std::vector<std::string> names;
		names.reserve(trace.size());

		// Skip the frames that raise and dispatch the exception, and the CRT/OS ones under main
		size_t first = 0;
		size_t last  = trace.size();
		for (size_t i = 0; i < trace.size(); ++i)
		{
			const std::string& name = names.emplace_back(trace[i].description());
			if (name.contains("CxxThrowException") || name.contains("KiUserExceptionDispatcher"))
			{
				first = i + 1;
			}
			if (name.contains("invoke_main") || name.contains("BaseThreadInitThunk"))
			{
				last = i;
				break;
			}
		}
		if (first >= last) first = 0;

		std::string out;
		for (size_t i = first; i < last; ++i)
		{
			out += std::format("\t#{:<3}{}", i - first, names[i]);
			if (const u32 line = trace[i].source_line(); line != 0)
			{
				out += std::format("  ({}:{})", fs::path(trace[i].source_file()).filename().string(), line);
			}
			out += '\n';
		}
		if (!out.empty()) out.pop_back();
		return out;
	}
}
