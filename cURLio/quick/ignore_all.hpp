/**
 * @file
 *
 * Convenience function to ignore all content from a device.
 */
#pragma once

#include "../detail/asio_include.hpp"

#include <array>

namespace cURLio::quick {

inline auto async_ignore_all(auto& stream, auto&& token)
{
	return CURLIO_ASIO_NS::async_compose<decltype(token), void(detail::asio_error_code, std::size_t)>(
	  [&stream, total = std::size_t{ 0 }, buffer = std::array<std::uint8_t, 4096>{}](
	    auto& self, const detail::asio_error_code& ec = {}, std::size_t bytes_transferred = 0) mutable {
		  total += bytes_transferred;
		  if (ec) {
			  self.complete(ec == CURLIO_ASIO_NS::error::eof ? detail::asio_error_code{} : ec, total);
		  } else {
			  stream.async_read_some(CURLIO_ASIO_NS::buffer(buffer), std::move(self));
		  }
	  },
	  token, stream);
}

} // namespace cURLio::quick
