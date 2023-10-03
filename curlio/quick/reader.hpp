/**
 * @file
 *
 * Convenience function to read all the response body to a `std::string`.
 */
#pragma once

#include "../basic_response.hpp"
#include "../error.hpp"
#include "../log.hpp"

#include <string>

namespace curlio::quick {

template<typename Executor>
inline auto async_read_all(std::shared_ptr<Basic_response<Executor>> response, auto&& token,
                           std::size_t buffer_increment = 4096)
{
	return CURLIO_ASIO_NS::async_compose<decltype(token), void(detail::asio_error_code, std::string)>(
	  [response = std::move(response), buffer_increment, last_buffer_size = std::size_t{ 0 },
	   buffer = std::string{}](auto& self, const detail::asio_error_code& ec = {},
	                           std::size_t bytes_transferred = 0) mutable {
		  last_buffer_size += bytes_transferred;
		  if (ec || ec == CURLIO_ASIO_NS::error::eof) {
			  buffer.resize(last_buffer_size);
			  self.complete(ec == CURLIO_ASIO_NS::error::eof ? detail::asio_error_code{} : ec, std::move(buffer));
		  } else {
			  // Initial call.
			  if (curl_off_t length;
			      bytes_transferred == 0 &&
			      (length = response->template get_info<CURLINFO_CONTENT_LENGTH_DOWNLOAD_T>()) >= 0) {
				  buffer.resize(static_cast<std::size_t>(length));
			  }

			  // Always make sure at least buffer_increment is available.
			  if (buffer.size() - last_buffer_size < buffer_increment) {
				  buffer.resize(last_buffer_size + buffer_increment);
			  }

			  response->async_read_some(
			    CURLIO_ASIO_NS::buffer(buffer.data() + last_buffer_size, buffer.size() - last_buffer_size),
			    std::move(self));
		  }
	  },
	  token, response->get_executor());
}

} // namespace curlio::quick
