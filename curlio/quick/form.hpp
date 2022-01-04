#pragma once

#include "../request.hpp"

#include <boost/asio.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <map>
#include <sstream>
#include <string_view>

namespace curlio::quick {

inline std::string construct_form(Request& request,
                                  const std::map<std::string_view, std::string_view>& parameters) noexcept
{
	std::stringstream ss;
	bool first = true;
	for (const auto& [key, value] : parameters) {
		if (!first) {
			ss << '&';
		}
		first = false;

		auto escaped =
		  curl_easy_escape(request.native_handle(), key.data(), boost::numeric_cast<int>(key.size()));
		ss << escaped << '=';
		curl_free(escaped);

		escaped = curl_easy_escape(request.native_handle(), value.data(), boost::numeric_cast<int>(value.size()));
		ss << escaped;
		curl_free(escaped);
	}
	return ss.str();
}

} // namespace curlio::quick
