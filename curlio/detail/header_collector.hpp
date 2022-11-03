#pragma once

#include "../error.hpp"
#include "case_insensitive_less.hpp"

#include <boost/algorithm/string.hpp>
#include <curl/curl.h>
#include <map>
#include <string>

namespace curlio::detail {

/// Hooks into the header callbacks of cURL and parses the header fields. Hook management must be done
/// separately.
class Header_collector {
public:
	using fields_type = std::map<std::string, std::string, Case_insensitive_less>;

	Header_collector(CURL* handle) : _handle{ handle }
	{
		curl_easy_setopt(_handle, CURLOPT_HEADERFUNCTION, &Header_collector::_header_callback);
		curl_easy_setopt(_handle, CURLOPT_HEADERDATA, this);
	}
	Header_collector(const Header_collector& copy) = delete;
	~Header_collector() noexcept
	{
		curl_easy_setopt(_handle, CURLOPT_HEADERFUNCTION, nullptr);
		curl_easy_setopt(_handle, CURLOPT_HEADERDATA, nullptr);
	}
	const fields_type& fields() const noexcept { return _fields; }
	void finish()
	{
		if (_headers_received_handler) {
			_headers_received_handler(boost::asio::error::eof);
			_headers_received_handler.reset();
		}
	}
	auto async_wait(auto&& fallback_executor, auto&& token)
	{
		return boost::asio::async_initiate<decltype(token), void(boost::system::error_code)>(
		  [this](auto handler, auto&& fallback_executor) {
			  auto executor = boost::asio::get_associated_executor(
			    handler, std::forward<decltype(fallback_executor)>(fallback_executor));

			  // Already received.
			  if (_ready_to_await) {
				  _ready_to_await = false;
				  boost::asio::post(std::move(executor), std::bind(std::move(handler), boost::system::error_code{}));
			  } else if (_headers_received_handler) {
				  boost::asio::post(std::move(executor),
				                    std::bind(std::move(handler), make_error_code(Code::multiple_headers_awaitings)));
			  } // Need to wait.
			  else {
				  _headers_received_handler = [this, executor = std::move(executor),
				                               handler = std::move(handler)](boost::system::error_code ec) mutable {
					  _ready_to_await = false;
					  boost::asio::post(std::move(executor), std::bind(std::move(handler), ec));
				  };
			  }
		  },
		  token, std::forward<decltype(fallback_executor)>(fallback_executor));
	}

private:
	fields_type _fields;
	CURL* _handle;
	std::uint8_t _last_clear       = 0;
	std::uint8_t _headers_received = 0;
	bool _ready_to_await           = false;
	Function<void(boost::system::error_code)> _headers_received_handler;

	static std::size_t _header_callback(char* buffer, std::size_t size, std::size_t count,
	                                    void* self_ptr) noexcept
	{
		const auto self                = static_cast<Header_collector*>(self_ptr);
		const std::size_t total_length = size * count;

		// New header segment -> clear old fields.
		if (self->_last_clear < self->_headers_received) {
			self->_fields.clear();
			self->_last_clear++;
		}

		// Add header to map.
		const auto end       = buffer + total_length;
		const auto seperator = std::find(buffer, end, ':');
		if (seperator != end) {
			std::string key{ buffer, static_cast<std::size_t>(seperator - buffer) };
			std::string value{ seperator + 1, static_cast<std::size_t>(end - seperator - 1) };
			boost::algorithm::trim(value);
			self->_fields.insert({ std::move(key), std::move(value) });
		}

		// End of header.
		if (total_length == 2) {
			self->_headers_received++;
			self->_ready_to_await = true;
			if (self->_headers_received_handler) {
				self->_headers_received_handler({});
				self->_headers_received_handler.reset();
			}
		}

		return total_length;
	}
};

} // namespace curlio::detail
