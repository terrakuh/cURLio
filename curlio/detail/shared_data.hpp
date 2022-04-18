#pragma once

#include "function.hpp"

#include <boost/asio.hpp>
#include <curl/curl.h>

namespace curlio::detail {

enum Status
{
	finished         = 0x1,
	headers_finished = 0x2,
};

/// Contains data shared between Request and Response.
class Shared_data
{
public:
	boost::asio::any_io_executor executor;
	CURL* const handle = curl_easy_init();
	Function<void()> request_finisher;
	Function<void()> response_finisher;
	int pause_mask = 0;
	int status     = 0;

	Shared_data()                   = default;
	Shared_data(Shared_data&& move) = delete;
	~Shared_data() noexcept
	{
		if (request_finisher) {
			request_finisher();
		}
		if (response_finisher) {
			response_finisher();
		}
		curl_easy_cleanup(handle);
	}

	Shared_data& operator=(Shared_data&& move) = delete;
};

} // namespace curlio::detail
