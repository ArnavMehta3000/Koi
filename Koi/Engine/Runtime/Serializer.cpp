#include "Koi/Engine/Runtime/Serializer.h"
#include <charconv>

namespace koi
{
	f64 ToTomlFloat(f32 value)
	{
		// to_chars with no precision gives the shortest text that round-trips back to this exact
		// f32, and a double is always wide enough to hold it, so the value survives unchanged
		char buffer[64];

		const auto [end, writeError] = std::to_chars(std::begin(buffer), std::end(buffer), value);
		if (writeError != std::errc{}) return static_cast<f64>(value);

		f64 out = 0.0;
		if (std::from_chars(std::begin(buffer), end, out).ec != std::errc{}) return static_cast<f64>(value);

		return out;
	}
}
