#pragma once

#include "detail/function.hpp"
#include "error.hpp"
#include "log.hpp"

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <cstdio>
#include <curl/curl.h>
#include <map>
#include <string>
#include <string_view>
#include <utility>

namespace curlio {

class Session;

class Request
{
public:
	typedef boost::asio::any_io_executor executor_type;

	/// This constructor assumes ownership of the provided handle.
	Request(CURL* handle = curl_easy_init()) noexcept;
	Request(Request&& move) = delete;
	~Request() noexcept;

	void set_url(const char* url) noexcept { curl_easy_setopt(_handle, CURLOPT_URL, url); }
	void append_http_field(const char* field) noexcept;
	void set_method(const char* method) noexcept;
	/// Returns the content length of the response or `-1` if unknown.
	curl_off_t content_length() noexcept;
	/// Returns the content type of the response. The string will get freed when `this` dies.
	std::string_view content_type() noexcept;
	void set_content_length(curl_off_t length) noexcept
	{
		curl_easy_setopt(_handle, CURLOPT_POSTFIELDSIZE_LARGE, length);
	}
	bool is_valid() const noexcept { return _handle != nullptr; }
	bool is_active() const noexcept { return is_valid() && static_cast<bool>(_executor); }
	CURL* native_handle() noexcept { return _handle; }
	template<typename Token>
	auto async_await_headers(Token&& token);
	/// Waits until the request is complete. Data must be read before this function.
	template<typename Token>
	auto async_await_completion(Token&& token);
	template<typename Mutable_buffer_sequence, typename Token>
	auto async_read_some(const Mutable_buffer_sequence& buffers, Token&& token);
	template<typename Const_buffer_sequence, typename Token>
	auto async_write_some(const Const_buffer_sequence& buffers, Token&& token);
	boost::asio::any_io_executor get_executor() noexcept { return _executor; }
	void swap(Request& other) noexcept;
	Request& operator=(Request&& move) = delete;

private:
	friend Session;

	enum Status
	{
		finished         = 0x1,
		headers_received = 0x2,
	};

	/// Set to the session's executor.
	boost::asio::any_io_executor _executor;
	std::map<std::string, std::string> _response_headers;
	detail::Function<void(boost::system::error_code)> _headers_received_handler;
	/// This handler is set when an asynchronous action waits for data.
	detail::Function<void(boost::system::error_code)> _write_handler;
	detail::Function<std::size_t(boost::system::error_code, void*, std::size_t)> _read_handler;
	int _pause_mask             = 0;
	CURL* _handle               = nullptr;
	curl_slist* _output_headers = nullptr;
	/// Stores one `_write_callback` call.
	boost::asio::streambuf _input_buffer;
	/// This handler is set when an asynchronous action waits for the request to complete.
	detail::Function<void()> _finish_handler;
	/// Contains status information about this request.
	int _status = 0;

	void _start();
	/// Marks this request as finished.
	void _finish();
	static std::size_t _write_callback(void* data, std::size_t size, std::size_t count,
	                                   void* self_pointer) noexcept;
	static std::size_t _read_callback(void* data, std::size_t size, std::size_t count,
	                                  void* self_pointer) noexcept;
	static std::size_t _header_callback(char* buffer, std::size_t size, std::size_t count,
	                                    void* self_pointer) noexcept;
};

inline Request::Request(CURL* handle) noexcept
{
	_handle = handle;
	if (_handle != nullptr) {
		curl_easy_setopt(_handle, CURLOPT_WRITEFUNCTION, &Request::_write_callback);
		curl_easy_setopt(_handle, CURLOPT_WRITEDATA, this);
		curl_easy_setopt(_handle, CURLOPT_READFUNCTION, &Request::_read_callback);
		curl_easy_setopt(_handle, CURLOPT_READDATA, this);
		curl_easy_setopt(_handle, CURLOPT_HEADERFUNCTION, &Request::_header_callback);
		curl_easy_setopt(_handle, CURLOPT_HEADERDATA, this);
		curl_easy_setopt(_handle, CURLOPT_PRIVATE, this);
		// enable all encodings
		curl_easy_setopt(_handle, CURLOPT_ACCEPT_ENCODING, "");
	}
}

inline Request::~Request() noexcept
{
	if (_handle != nullptr) {
		curl_easy_cleanup(_handle);
		curl_slist_free_all(_output_headers);
	}
}

inline void Request::append_http_field(const char* field) noexcept
{
	_output_headers = curl_slist_append(_output_headers, field);
	curl_easy_setopt(_handle, CURLOPT_HTTPHEADER, _output_headers);
}

inline void Request::set_method(const char* method) noexcept
{
	std::string_view tmp = method;
	if (boost::algorithm::iequals(tmp, "get")) {
		curl_easy_setopt(_handle, CURLOPT_HTTPGET, 1);
	} else if (boost::algorithm::iequals(tmp, "post")) {
		curl_easy_setopt(_handle, CURLOPT_POST, 1);
	} else {
		curl_easy_setopt(_handle, CURLOPT_CUSTOMREQUEST, method);
	}
}

inline curl_off_t Request::content_length() noexcept
{
	curl_off_t value = -1;
	if (curl_easy_getinfo(_handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &value) != CURLE_OK) {
		return -1;
	}
	return value;
}

inline std::string_view Request::content_type() noexcept
{
	char* value = nullptr;
	curl_easy_getinfo(_handle, CURLINFO_CONTENT_TYPE, &value);
	return { value, value == nullptr ? 0 : std::char_traits<char>::length(value) };
}

template<typename Token>
inline auto Request::async_await_headers(Token&& token)
{
	return boost::asio::async_initiate<Token, void(boost::system::error_code)>(
	  [this](auto handler) {
		  if (_status & headers_received) {
			  std::move(handler)(boost::system::error_code{});
		  } else if (_headers_received_handler) {
			  std::move(handler)(Code::multiple_headers_awaitings);
		  } else {
			  _headers_received_handler = [this,
			                               handler = std::move(handler)](boost::system::error_code ec) mutable {
				  auto executor = boost::asio::get_associated_executor(handler);
				  boost::asio::post(executor,
				                    [handler = std::move(handler), ec]() mutable { std::move(handler)(ec); });
			  };
		  }
	  },
	  token);
}

template<typename Token>
inline auto Request::async_await_completion(Token&& token)
{
	return boost::asio::async_initiate<Token, void(boost::system::error_code)>(
	  [this](auto handler) {
		  if (_status & finished) {
			  std::move(handler)(boost::system::error_code{});
		  } else if (_finish_handler) {
			  std::move(handler)(Code::multiple_completion_awaitings);
		  } else {
			  _finish_handler = [this, handler = std::move(handler)]() mutable {
				  _finish();
				  auto executor = boost::asio::get_associated_executor(handler);
				  boost::asio::post(executor, [handler = std::move(handler)]() mutable {
					  std::move(handler)(boost::system::error_code{});
				  });
			  };
		  }
	  },
	  token);
}

template<typename Mutable_buffer_sequence, typename Token>
inline auto Request::async_read_some(const Mutable_buffer_sequence& buffers, Token&& token)
{
	CURLIO_TRACE("Reading some");
	return boost::asio::async_initiate<Token, void(boost::system::error_code, std::size_t)>(
	  [this, buffers](auto handler) {
		  // can immediately finish
		  if (_input_buffer.size() > 0) {
			  const std::size_t copied = boost::asio::buffer_copy(buffers, _input_buffer.data());
			  _input_buffer.consume(copied);
			  std::move(handler)(boost::system::error_code{}, copied);
		  } else if (_write_handler) {
			  std::move(handler)(Code::multiple_reads, 0);
		  } else if (_status & finished) {
			  std::move(handler)(boost::asio::error::eof, 0);
		  } else {
			  // set write handler when cURL calls the write callback
			  _write_handler = [this, buffers, handler = std::move(handler)](boost::system::error_code ec) mutable {
				  std::size_t copied = 0;
				  // copy data and finish
				  if (!ec) {
					  copied = boost::asio::buffer_copy(buffers, _input_buffer.data());
					  _input_buffer.consume(copied);
					  CURLIO_DEBUG("Read " << copied << " bytes");
				  }
				  auto executor = boost::asio::get_associated_executor(handler);
				  boost::asio::post(executor, [handler = std::move(handler), ec, copied]() mutable {
					  std::move(handler)(ec, copied);
				  });
			  };

			  // TODO check for errors
			  _pause_mask &= ~CURLPAUSE_RECV;
			  curl_easy_pause(_handle, _pause_mask);
		  }
	  },
	  token);
}

template<typename Const_buffer_sequence, typename Token>
inline auto Request::async_write_some(const Const_buffer_sequence& buffers, Token&& token)
{
	return boost::asio::async_initiate<Token, void(boost::system::error_code, std::size_t)>(
	  [this, buffers](auto handler) {
		  // can immediately finish
		  if (_read_handler) {
			  std::move(handler)(Code::multiple_writes, 0);
		  } else if (_status & finished) {
			  std::move(handler)(boost::asio::error::eof, 0);
		  } else {
			  // set write handler when cURL calls the write callback
			  _read_handler = [this, buffers, handler = std::move(handler)](boost::system::error_code ec,
			                                                                void* data, std::size_t size) mutable {
				  // copy data and finish
				  const std::size_t copied = boost::asio::buffer_copy(boost::asio::buffer(data, size), buffers);
				  auto executor            = boost::asio::get_associated_executor(handler);
				  boost::asio::post(executor, [handler = std::move(handler), ec, copied]() mutable {
					  std::move(handler)(ec, copied);
				  });
				  return copied;
			  };

			  // TODO check for errors
			  _pause_mask &= ~CURLPAUSE_SEND;
			  curl_easy_pause(_handle, _pause_mask);
		  }
	  },
	  token);
}

inline void Request::_start() { _status = 0; }

inline void Request::_finish()
{
	CURLIO_DEBUG("Request " << this << " finished");
	_status |= finished;
	if (_headers_received_handler) {
		_headers_received_handler(boost::asio::error::eof);
		_headers_received_handler.reset();
	}
	if (_write_handler) {
		_write_handler(boost::asio::error::eof);
		_write_handler.reset();
	}
	if (_read_handler) {
		_read_handler(boost::asio::error::eof, nullptr, 0);
		_read_handler.reset();
	}
	if (_finish_handler) {
		_finish_handler();
		_finish_handler.reset();
	}
	_executor = {};
}

inline std::size_t Request::_write_callback(void* data, std::size_t size, std::size_t count,
                                            void* self_pointer) noexcept
{
	CURLIO_TRACE("Receiving callback with buffer size of " << size * count << " bytes");
	const auto self = static_cast<Request*>(self_pointer);

	// data is wanted
	if (self->_write_handler) {
		const std::size_t copied = boost::asio::buffer_copy(self->_input_buffer.prepare(size * count),
		                                                    boost::asio::buffer(data, size * count));
		self->_input_buffer.commit(copied);
		CURLIO_DEBUG("Received " << copied << " bytes");
		self->_write_handler({});
		self->_write_handler.reset();
		return copied;
	}

	CURLIO_TRACE("Pausing receiving function");
	self->_pause_mask |= CURLPAUSE_RECV;
	return CURL_WRITEFUNC_PAUSE;
}

inline std::size_t Request::_read_callback(void* data, std::size_t size, std::size_t count,
                                           void* self_pointer) noexcept
{
	CURLIO_TRACE("Sending callback with buffer size of " << size * count << " bytes");
	const auto self = static_cast<Request*>(self_pointer);

	if (self->_read_handler) {
		const std::size_t copied = self->_read_handler({}, data, size * count);
		self->_read_handler.reset();
		if (copied > 0) {
			CURLIO_DEBUG("Sending " << copied << " bytes");
			return copied;
		}
	}

	CURLIO_TRACE("Pausing sending function");
	self->_pause_mask |= CURLPAUSE_SEND;
	return CURL_READFUNC_PAUSE;
}

inline std::size_t Request::_header_callback(char* buffer, std::size_t size, std::size_t count,
                                             void* self_pointer) noexcept
{
	CURLIO_TRACE("Received " << size * count << " bytes of header data");
	const auto self = static_cast<Request*>(self_pointer);

	if (const char* seperator = std::find(buffer, buffer + size * count, ':');
	    seperator != buffer + size * count) {
		std::string key{ buffer, static_cast<std::size_t>(seperator - buffer) };
		std::string value{ seperator + 1, static_cast<std::size_t>(buffer + size * count - seperator - 1) };
		boost::algorithm::trim(value);
		self->_response_headers.insert({ std::move(key), std::move(value) });
	}

	if (size * count == 2) {
		puts("hihihihihi");
		// signal that all headers were received
		self->_status = headers_received;
		if (self->_headers_received_handler) {
			CURLIO_TRACE("Signaling headers received");
			self->_headers_received_handler({});
			self->_headers_received_handler.reset();
		}
	}

	return size * count;
}

} // namespace curlio
