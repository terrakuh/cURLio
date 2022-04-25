#pragma once

#include "../error.hpp"
#include "../log.hpp"
#include "function.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <cctype>
#include <curl/curl.h>
#include <map>
#include <string>

namespace curlio::detail {

struct Insensitive_less
{
	bool operator()(const std::string& lhs, const std::string& rhs) const noexcept
	{
		if (lhs.size() < rhs.size()) {
			return true;
		} else if (lhs.size() > rhs.size()) {
			return false;
		}
		for (std::size_t i = 0; i < lhs.size(); ++i) {
			const int l = std::toupper(static_cast<unsigned char>(lhs[i]));
			const int r = std::toupper(static_cast<unsigned char>(rhs[i]));
			if (l < r) {
				return true;
			} else if (l > r) {
				return false;
			}
		}
		return false;
	}
};

/// Hooks into the header callbacks of cURL and parses the header fields. Hook management must be done
/// separately.
class Header_collector
{
public:
	typedef std::map<std::string, std::string, Insensitive_less> Fields;

	Fields& fields() noexcept { return _fields; }
	const Fields& fields() const noexcept { return _fields; }
	void hook(CURL* handle) noexcept;
	void unhook(CURL* handle) noexcept;
	void finish();
	/// Waits until the header received flag is set. Clears it before completion.
	template<typename Token>
	auto async_await_headers(Token&& token);

private:
	Fields _fields;
	std::uint8_t _last_clear       = 0;
	std::uint8_t _headers_received = 0;
	bool _ready_to_await           = false;
	Function<void(boost::system::error_code)> _headers_received_handler;

	static std::size_t _header_callback(char* buffer, std::size_t size, std::size_t count,
	                                    void* self_pointer) noexcept;
};

inline void Header_collector::hook(CURL* handle) noexcept
{
	curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, &Header_collector::_header_callback);
	curl_easy_setopt(handle, CURLOPT_HEADERDATA, this);
}

inline void Header_collector::unhook(CURL* handle) noexcept
{
	curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, nullptr);
	curl_easy_setopt(handle, CURLOPT_HEADERDATA, nullptr);
}

inline void Header_collector::finish()
{
	if (_headers_received_handler) {
		_headers_received_handler(boost::asio::error::eof);
		_headers_received_handler.reset();
	}
}

template<typename Token>
inline auto Header_collector::async_await_headers(Token&& token)
{
	return boost::asio::async_initiate<Token, void(boost::system::error_code)>(
	  [this](auto handler) {
		  // already received
		  if (_ready_to_await) {
			  _ready_to_await = false;
			  std::move(handler)(boost::system::error_code{});
		  } else if (_headers_received_handler) {
			  std::move(handler)(Code::multiple_headers_awaitings);
		  } // need to wait
		  else {
			  _headers_received_handler = [this,
			                               handler = std::move(handler)](boost::system::error_code ec) mutable {
				  _ready_to_await = false;
				  auto executor   = boost::asio::get_associated_executor(handler);
				  boost::asio::post(executor,
				                    [handler = std::move(handler), ec]() mutable { std::move(handler)(ec); });
			  };
		  }
	  },
	  token);
}

inline std::size_t Header_collector::_header_callback(char* buffer, std::size_t size, std::size_t count,
                                                      void* self_pointer) noexcept
{
	CURLIO_TRACE("Received " << size * count << " bytes of header data");
	const auto self = static_cast<Header_collector*>(self_pointer);

	// new header -> clear old fields
	if (self->_last_clear < self->_headers_received) {
		self->_fields.clear();
		self->_last_clear++;
	}

	// add header to map
	const auto end       = buffer + size * count;
	const auto seperator = std::find(buffer, end, ':');
	if (seperator != end) {
		std::string key{ buffer, static_cast<std::size_t>(seperator - buffer) };
		std::string value{ seperator + 1, static_cast<std::size_t>(end - seperator - 1) };
		boost::algorithm::trim(value);
		CURLIO_DEBUG("Parsed header field: " << key << "=" << value);
		self->_fields.insert({ std::move(key), std::move(value) });
	}

	// end of header
	if (size * count == 2) {
		// signal that all headers were received
		self->_headers_received++;
		self->_ready_to_await = true;
		if (self->_headers_received_handler) {
			self->_headers_received_handler({});
			self->_headers_received_handler.reset();
		}
	}

	return size * count;
}

} // namespace curlio::detail
