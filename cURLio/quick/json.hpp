/**
 * @file
 * 
 * Quicky parse the request body as JSON or write to the response.
 */
#pragma once

#include "../error.hpp"
#include "../request.hpp"
#include "../response.hpp"

#include <array>
#include <boost/asio.hpp>
#include <boost/json.hpp>
#include <memory>

namespace cURLio::quick {

/**
 * Writes the given JSON value to the request object. No headers are modified nor is a size specified. The
 * handler signature is `void(boost::system::error_code, std::size_t)`
 *
 * @pre `request.is_active() == true`
 * @param request This object must live as long as this operation is running.
 * @param value This object must live as long as this operation is running.
 * @returns The amount of written bytes.
 */
template<typename Token>
inline auto async_write_json(Request& request, const boost::json::value& value, Token&& token)
{
	if (!request.is_active()) {
		throw boost::system::system_error{ Code::request_not_active };
	}

	auto serializer = std::make_unique<boost::json::serializer>();
	serializer->reset(&value);
	return boost::asio::async_compose<Token, void(boost::system::error_code, std::size_t)>(
	  [&, serializer = std::move(serializer), written = std::size_t{ 0 },
	   buffer = std::array<char, BOOST_JSON_STACK_BUFFER_SIZE>{}](auto& self, boost::system::error_code ec = {},
	                                                              std::size_t bytes_written = 0) mutable {
		  written += bytes_written;
		  if (serializer->done() || ec) {
			  self.complete(ec, written);
		  } else {
			  const auto view = serializer->read(buffer.data(), buffer.size());
			  boost::asio::async_write(request, boost::asio::buffer(view.data(), view.size()), std::move(self));
		  }
	  },
	  token, request);
}

/**
 * Reads a JSON object from the stream. Bytes read after the JSON are discarded. The handler signature is
 * `void(boost::system::error_code, boost::json::value)`.
 *
 * @pre `response.is_active() == true`
 * @param response This object must live as long as this operation is running.
 */
template<typename Token>
inline auto async_read_json(Response& response, Token&& token)
{
	if (!response.is_active()) {
		throw boost::system::system_error{ Code::request_not_active };
	}

	return boost::asio::async_compose<Token, void(boost::system::error_code, boost::json::value)>(
	  [&response, parser = std::make_unique<boost::json::stream_parser>(),
	   buffer = std::array<char, BOOST_JSON_STACK_BUFFER_SIZE>{}](auto& self, boost::system::error_code ec = {},
	                                                              std::size_t bytes_written = 0) mutable {
		  if (bytes_written > 0) {
			  parser->write(buffer.data(), bytes_written);
		  }
		  if (ec) {
			  self.complete(ec, boost::json::value{});
		  } else if (parser->done()) {
			  self.complete(boost::system::error_code{}, parser.release());
		  } else {
			  response.async_read_some(boost::asio::buffer(buffer), std::move(self));
		  }
	  },
	  token, response);
}

} // namespace cURLio::quick
