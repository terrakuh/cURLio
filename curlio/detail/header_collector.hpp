#pragma once

#include "../error.hpp"
#include "../log.hpp"
#include "asio_include.hpp"
#include "case_insensitive_less.hpp"
#include "function.hpp"

#include <curl/curl.h>
#include <locale>
#include <map>
#include <string>
#include <string_view>

namespace curlio::detail {

constexpr std::string_view trim(std::string_view str, const std::locale& locale = {})
{
	auto begin = str.begin();
	auto end   = str.end();
	for (; begin != end; ++begin) {
		if (!std::isspace(*begin, locale)) {
			break;
		}
	}
	for (; begin != end; --end) {
		if (!std::isspace(*(end - 1), locale)) {
			break;
		}
	}
	return { begin, end };
}

/// Hooks into the header callbacks of cURL and parses the header fields. Hook management must be done
/// separately.
class HeaderCollector {
public:
	using fields_type = std::map<std::string, std::string, Case_insensitive_less>;

	HeaderCollector(CURL* handle) noexcept : _handle{ handle } {}
	HeaderCollector(const HeaderCollector& copy) = delete;
	HeaderCollector(HeaderCollector&& move)      = delete;
	~HeaderCollector()                           = default;

	void start() noexcept
	{
		curl_easy_setopt(_handle, CURLOPT_HEADERFUNCTION, &HeaderCollector::_header_callback);
		curl_easy_setopt(_handle, CURLOPT_HEADERDATA, this);
	}
	void stop() noexcept
	{
		curl_easy_setopt(_handle, CURLOPT_HEADERFUNCTION, nullptr);
		curl_easy_setopt(_handle, CURLOPT_HEADERDATA, nullptr);
		if (_headers_received_handler) {
			_headers_received_handler(CURLIO_ASIO_NS::error::eof);
			_headers_received_handler.reset();
		}
	}
	auto async_wait(auto&& fallback_executor, auto&& token)
	{
		return CURLIO_ASIO_NS::async_initiate<decltype(token), void(asio_error_code, fields_type)>(
		  [this](auto handler, auto&& fallback_executor) {
			  auto executor = CURLIO_ASIO_NS::get_associated_executor(
			    handler, std::forward<decltype(fallback_executor)>(fallback_executor));

			  // Already received.
			  if (_ready_to_await) {
				  _ready_to_await = false;
				  CURLIO_ASIO_NS::post(std::move(executor),
				                       std::bind(std::move(handler), asio_error_code{}, std::move(_fields)));
			  } else if (_headers_received_handler) {
				  CURLIO_ASIO_NS::post(std::move(executor),
				                       std::bind(std::move(handler),
				                                 asio_error_code{ make_error_code(Code::multiple_headers_awaitings) },
				                                 fields_type{}));
			  } // Need to wait.
			  else {
				  _headers_received_handler = [this, executor = std::move(executor),
				                               handler = std::move(handler)](asio_error_code ec) mutable {
					  _ready_to_await = false;
					  CURLIO_ASIO_NS::post(std::move(executor), std::bind(std::move(handler), ec, std::move(_fields)));
				  };
			  }
		  },
		  token, std::forward<decltype(fallback_executor)>(fallback_executor));
	}

	HeaderCollector& operator=(const HeaderCollector& copy) = delete;
	HeaderCollector& operator=(HeaderCollector&& move)      = delete;

private:
	fields_type _fields;
	CURL* _handle;
	std::uint8_t _last_clear       = 0;
	std::uint8_t _headers_received = 0;
	bool _ready_to_await           = false;
	Function<void(asio_error_code)> _headers_received_handler;

	static std::size_t _header_callback(char* buffer, std::size_t size, std::size_t count,
	                                    void* self_ptr) noexcept
	{
		static std::locale locale{};
		const auto self                = static_cast<HeaderCollector*>(self_ptr);
		const std::size_t total_length = size * count;

		CURLIO_TRACE("Received " << total_length << " bytes of headers");

		// New header segment -> clear old fields.
		if (self->_last_clear < self->_headers_received) {
			CURLIO_TRACE("Cleaning headers because of new segment");
			self->_fields.clear();
			self->_last_clear++;
		}

		// Add header to map.
		const auto end       = buffer + total_length;
		const auto separator = std::find(buffer, end, ':');
		if (separator != end) {
			std::string key{ buffer, static_cast<std::size_t>(separator - buffer) };
			std::string value{ trim({ separator + 1, static_cast<std::size_t>(end - separator - 1) }, locale) };
			self->_fields.insert({ std::move(key), std::move(value) });
		}

		// End of header.
		if (total_length == 2) {
			CURLIO_TRACE("End of header: waiter=" << static_cast<bool>(self->_headers_received_handler));
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
