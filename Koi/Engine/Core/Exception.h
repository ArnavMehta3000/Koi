#pragma once
#include <stacktrace>
#include <stdexcept>
#include <string>

namespace koi
{
	// Carries the stack of where the error really happened when it's rethrown somewhere else
	class Exception : public std::runtime_error
	{
	public:
		explicit Exception(const std::string& message, std::stacktrace trace = {})
			: std::runtime_error(message), m_trace(std::move(trace)) {}

		[[nodiscard]] const std::stacktrace& GetTrace() const noexcept { return m_trace; }

	private:
		std::stacktrace m_trace;
	};
}
