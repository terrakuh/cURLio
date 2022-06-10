#pragma once

#include "../error.hpp"
#include "../response.hpp"

#include <boost/asio.hpp>
#include <string>

namespace curlio::quick {

/**
 * Reads the content from the given stream into a string.The handler signature is
 * `void(boost::system::error_code, std::string)`.
 *
 * @pre `response.is_active() == true`
 * @param response This object must live as long as this operation is running.
 */
template<typename Token>
inline auto async_read_all(Response& response, Token&& token)
{
	return boost::asio::async_compose<Token, void(boost::system::error_code, std::string)>(
	  [&response, last_buffer_size = std::size_t{ 0 }, has_limit = false, buffer = std::string{}](
	    auto& self, boost::system::error_code ec = {}, std::size_t bytes_read = 0) mutable {
		  if (!ec && bytes_read == 0 && !response.is_active()) {
			  ec = Code::request_not_active;
		  }

		  last_buffer_size += bytes_read;
		  if (ec) {
			  buffer.resize(last_buffer_size);
			  self.complete(ec == boost::asio::error::eof ? boost::system::error_code{} : ec, std::move(buffer));
		  } else {
			  if (bytes_read == 0) {
				  const auto length = response.content_length();
				  CURLIO_DEBUG("Content length for reading " << length);
				  if (length >= 0) {
					  has_limit = true;
					  buffer.resize(static_cast<std::size_t>(length));
				  }
			  }

			  if (buffer.size() - last_buffer_size < 4096) {
				  buffer.resize(last_buffer_size + 4096);
			  }

			  response.async_read_some(
			    boost::asio::buffer(buffer.data() + last_buffer_size, buffer.size() - last_buffer_size),
			    std::move(self));
		  }
	  },
	  token, response.get_executor());
}

} // namespace curlio::quick
