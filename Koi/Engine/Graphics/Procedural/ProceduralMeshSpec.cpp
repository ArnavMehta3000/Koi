#include "Koi/Engine/Graphics/Procedural/ProceduralMeshSpec.h"
#include <charconv>
#include <format>
#include <variant>

namespace koi
{
	namespace
	{
		// toml::node is abstract, so scalars travel as a variant and get inserted by visit
		using ScalarValue = std::variant<bool, i64, f64, std::string>;

		// Similar logic to ToTOMLFloat in Serializer.h
		[[nodiscard]] std::string FormatFloat(f64 value)
		{
			const f32 narrowed = static_cast<f32>(value);

			std::string out = static_cast<f64>(narrowed) == value
				? std::format("{}", narrowed)
				: std::format("{}", value);

			if (out.find_first_of(".eEni") == std::string::npos)
				out += ".0";

			return out;
		}

		[[nodiscard]] std::string FormatScalar(const toml::node& node)
		{
			if (const auto* asBool  = node.as_boolean())        return asBool->get() ? "true" : "false";
			if (const auto* asInt   = node.as_integer())        return std::format("{}", asInt->get());
			if (const auto* asFloat = node.as_floating_point()) return FormatFloat(asFloat->get());
			if (const auto* asStr   = node.as_string())         return asStr->get();

			return {};
		}

		// Inverse of FormatScalar. The type is inferred from the text, which round-trips exactly because FormatScalar is the only thing that ever writes it
		[[nodiscard]] ScalarValue ParseScalar(std::string_view text)
		{
			if (text == "true")  return true;
			if (text == "false") return false;

			const char* begin = text.data();
			const char* end   = text.data() + text.size();

			if (text.find_first_of(".eEni") == std::string_view::npos)
			{
				i64 asInt = 0;
				if (std::from_chars(begin, end, asInt).ptr == end)
					return asInt;
			}
			else
			{
				f64 asFloat = 0.0;
				if (std::from_chars(begin, end, asFloat).ptr == end)
					return asFloat;
			}

			return std::string(text);
		}

		[[nodiscard]] std::string FormatValue(const toml::node& node)
		{
			const toml::array* arr = node.as_array();
			if (!arr) return FormatScalar(node);

			std::string out;
			for (const toml::node& elem : *arr)
			{
				if (!out.empty()) out += ',';
				out += FormatScalar(elem);
			}

			return out;
		}

		void InsertValue(toml::table& table, std::string_view key, std::string_view text)
		{
			if (text.find(',') == std::string_view::npos)
			{
				std::visit([&](auto&& value) { table.insert(key, std::forward<decltype(value)>(value)); },
					ParseScalar(text));
				return;
			}

			toml::array arr;
			for (const auto part : std::views::split(text, ','))
			{
				std::visit([&](auto&& value) { arr.push_back(std::forward<decltype(value)>(value)); },
					ParseScalar(std::string_view(part)));
			}

			table.insert(key, std::move(arr));
		}
	}

	std::string ToProceduralUri(std::string_view generator, const toml::table& params)
	{
		std::string out(kProceduralScheme);
		out += generator;

		char separator = '?';
		for (const auto& [key, value] : params)
		{
			out += separator;
			out += key.str();
			out += '=';
			out += FormatValue(value);

			separator = '&';
		}

		return out;
	}

	std::optional<std::pair<std::string, toml::table>> ParseProceduralUri(std::string_view uri)
	{
		// Floats always carry a decimal point and integers never do, which is what lets the parsing recover the node types the params struct expects
		if (!uri.starts_with(kProceduralScheme)) return std::nullopt;

		const std::string_view rest = uri.substr(kProceduralScheme.size());
		const size_t queryStart = rest.find('?');

		std::string generator(rest.substr(0, queryStart));
		if (generator.empty()) return std::nullopt;

		toml::table params;

		// No query at all is legal: every param falls back to its default
		if (queryStart != std::string_view::npos)
		{
			for (const auto pair : std::views::split(rest.substr(queryStart + 1), '&'))
			{
				const std::string_view text{ pair };
				const size_t split = text.find('=');
				if (split == std::string_view::npos || split == 0) continue;

				InsertValue(params, text.substr(0, split), text.substr(split + 1));
			}
		}

		return std::pair{ std::move(generator), std::move(params) };
	}
}
