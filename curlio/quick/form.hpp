#pragma once

#include <boost/asio.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <curl/curl.h>
#include <map>
#include <sstream>
#include <string_view>

namespace curlio::quick {

/// Constructs a query parameter list from the given associative container (like `std::map`).
template<typename Associative_container>
inline std::string construct_form(CURL* handle, const Associative_container& parameters) noexcept
{
	std::stringstream ss;
	bool first = true;
	for (const auto& [key, value] : parameters) {
		if (!first) {
			ss << '&';
		}
		first = false;

		auto escaped = curl_easy_escape(handle, key.data(), boost::numeric_cast<int>(key.size()));
		ss << escaped << '=';
		curl_free(escaped);

		escaped = curl_easy_escape(handle, value.data(), boost::numeric_cast<int>(value.size()));
		ss << escaped;
		curl_free(escaped);
	}
	return ss.str();
}

} // namespace curlio::quick
