/**
 * @file
 *
 * Convenience function for form generation.
 */
#pragma once

#include "../detail/final_action.hpp"

#include <curl/curl.h>
#include <functional>
#include <map>
#include <sstream>

namespace cURLio::quick {

/// Constructs a query parameter list from the given associative container (like `std::map`).
template<typename Associative_container = std::map<std::string, std::string>>
inline std::string construct_form(CURL* handle, const Associative_container& parameters)
{
	std::stringstream ss;
	bool first = true;
	for (const auto& [key, value] : parameters) {
		if (!first) {
			ss << '&';
		}
		first = false;

		auto escaped  = curl_easy_escape(handle, key.data(), key.size());
		const auto _0 = detail::finally(std::bind(curl_free, escaped));
		ss << escaped << '=';

		escaped       = curl_easy_escape(handle, value.data(), value.size());
		const auto _1 = detail::finally(std::bind(curl_free, escaped));
		ss << escaped;
	}
	return ss.str();
}

} // namespace cURLio::quick
