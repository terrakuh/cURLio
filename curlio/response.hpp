#pragma once

#include "detail/function.hpp"
#include "detail/shared_data.hpp"
#include "error.hpp"
#include "log.hpp"

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <cctype>
#include <cstdio>
#include <curl/curl.h>
#include <map>
#include <string>
#include <string_view>
#include <utility>

namespace curlio {

class Session;

struct Insensitive_less
{
	bool operator()(const std::string& lhs, const std::string& rhs) const
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

class Response
{
public:
	typedef boost::asio::any_io_executor executor_type;
	typedef std::map<std::string, std::string, Insensitive_less> Headers;

	Response(Response&& move) = delete;
	~Response() noexcept;

	/// Returns the content length of the response or `-1` if unknown.
	curl_off_t content_length() noexcept;
	/// Returns the content type of the response. The string will get freed when `this` dies.
	std::string_view content_type() noexcept;
	bool is_valid() const noexcept { return !_data.expired(); }
	bool is_active() const noexcept { return is_valid(); }
	CURL* native_handle() noexcept;
	Headers& headers() noexcept { return _headers; }
	template<typename Token>
	auto async_await_headers(Token&& token);
	/// Waits until the response is complete. Data must be read before this function.
	template<typename Token>
	auto async_await_completion(Token&& token);
	template<typename Mutable_buffer_sequence, typename Token>
	auto async_read_some(const Mutable_buffer_sequence& buffers, Token&& token);
	/// Getting the executor of an invalid response is undefined behavior.
	boost::asio::any_io_executor get_executor() noexcept { return _data.lock()->executor; }
	Response& operator=(Response&& move) = delete;

private:
	friend Session;

	std::weak_ptr<detail::Shared_data> _data;
	Headers _headers;
	detail::Function<void(boost::system::error_code)> _headers_received_handler;
	/// This handler is set when an asynchronous action waits for data.
	detail::Function<void(boost::system::error_code)> _receive_handler;
	detail::Function<void()> _finish_handler;
	/// This buffer is required when more data is received than read.
	boost::asio::streambuf _input_buffer;

	Response(const std::shared_ptr<detail::Shared_data>& data) noexcept;
	static std::size_t _receive_callback(void* data, std::size_t size, std::size_t count,
	                                     void* self_pointer) noexcept;
	static std::size_t _header_callback(char* buffer, std::size_t size, std::size_t count,
	                                    void* self_pointer) noexcept;
};

inline Response::~Response() noexcept
{
	const auto handle = native_handle();
	if (handle != nullptr) {
		curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, nullptr);
		curl_easy_setopt(handle, CURLOPT_WRITEDATA, nullptr);
		curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, nullptr);
		curl_easy_setopt(handle, CURLOPT_HEADERDATA, nullptr);
	}
}

inline curl_off_t Response::content_length() noexcept
{
	curl_off_t value = -1;
	if (curl_easy_getinfo(native_handle(), CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &value) != CURLE_OK) {
		return -1;
	}
	return value;
}

inline std::string_view Response::content_type() noexcept
{
	char* value     = nullptr;
	const auto code = curl_easy_getinfo(native_handle(), CURLINFO_CONTENT_TYPE, &value);
	return { value, code != CURLE_OK || value == nullptr ? 0 : std::char_traits<char>::length(value) };
}

inline CURL* Response::native_handle() noexcept
{
	const auto ptr = _data.lock();
	return ptr == nullptr ? nullptr : ptr->handle;
}

template<typename Token>
inline auto Response::async_await_headers(Token&& token)
{
	return boost::asio::async_initiate<Token, void(boost::system::error_code)>(
	  [this](auto handler) {
		  const auto ptr = _data.lock();
		  if (ptr->status & detail::headers_finished) {
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
inline auto Response::async_await_completion(Token&& token)
{
	return boost::asio::async_initiate<Token, void(boost::system::error_code)>(
	  [this](auto handler) {
		  const auto ptr = _data.lock();
		  if (ptr == nullptr || ptr->status & detail::finished) {
			  std::move(handler)(boost::system::error_code{});
		  } else if (_finish_handler) {
			  std::move(handler)(Code::multiple_completion_awaitings);
		  } else {
			  _finish_handler = [handler = std::move(handler)]() mutable {
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
inline auto Response::async_read_some(const Mutable_buffer_sequence& buffers, Token&& token)
{
	CURLIO_TRACE("Reading some");
	return boost::asio::async_initiate<Token, void(boost::system::error_code, std::size_t)>(
	  [this, buffers](auto handler) {
		  const auto ptr = _data.lock();

		  // can immediately finish
		  if (_input_buffer.size() > 0) {
			  const std::size_t copied = boost::asio::buffer_copy(buffers, _input_buffer.data());
			  _input_buffer.consume(copied);
			  std::move(handler)(boost::system::error_code{}, copied);
		  } else if (_receive_handler) {
			  std::move(handler)(Code::multiple_reads, 0);
		  } else if (ptr == nullptr || ptr->status & detail::finished) {
			  std::move(handler)(boost::asio::error::eof, 0);
		  } else {
			  // set write handler when cURL calls the write callback
			  _receive_handler = [this, buffers,
			                      handler = std::move(handler)](boost::system::error_code ec) mutable {
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
			  ptr->pause_mask &= ~CURLPAUSE_RECV;
			  curl_easy_pause(ptr->handle, ptr->pause_mask);
		  }
	  },
	  token);
}

inline Response::Response(const std::shared_ptr<detail::Shared_data>& data) noexcept : _data{ data }
{
	curl_easy_setopt(data->handle, CURLOPT_WRITEFUNCTION, &Response::_receive_callback);
	curl_easy_setopt(data->handle, CURLOPT_WRITEDATA, this);
	curl_easy_setopt(data->handle, CURLOPT_HEADERFUNCTION, &Response::_header_callback);
	curl_easy_setopt(data->handle, CURLOPT_HEADERDATA, this);
	data->response_finisher = [this] {
		CURLIO_TRACE("Response " << this << " finished");
		if (_headers_received_handler) {
			_headers_received_handler(boost::asio::error::eof);
			_headers_received_handler.reset();
		}
		if (_receive_handler) {
			_receive_handler(boost::asio::error::eof);
			_receive_handler.reset();
		}
		if (_finish_handler) {
			_finish_handler();
			_finish_handler.reset();
		}
	};
}

inline std::size_t Response::_receive_callback(void* data, std::size_t size, std::size_t count,
                                               void* self_pointer) noexcept
{
	CURLIO_TRACE("Receiving callback with buffer size of " << size * count << " bytes");
	const auto self = static_cast<Response*>(self_pointer);

	// data is wanted
	if (self->_receive_handler) {
		const std::size_t copied = boost::asio::buffer_copy(self->_input_buffer.prepare(size * count),
		                                                    boost::asio::buffer(data, size * count));
		self->_input_buffer.commit(copied);
		CURLIO_DEBUG("Received " << copied << " bytes");
		self->_receive_handler({});
		self->_receive_handler.reset();
		return copied;
	}

	CURLIO_TRACE("Pausing receiving function");
	self->_data.lock()->pause_mask |= CURLPAUSE_RECV;
	return CURL_WRITEFUNC_PAUSE;
}

inline std::size_t Response::_header_callback(char* buffer, std::size_t size, std::size_t count,
                                              void* self_pointer) noexcept
{
	CURLIO_TRACE("Received " << size * count << " bytes of header data");
	const auto self = static_cast<Response*>(self_pointer);

	const auto end       = buffer + size * count;
	const auto seperator = std::find(buffer, end, ':');
	if (seperator != end) {
		std::string key{ buffer, static_cast<std::size_t>(seperator - buffer) };
		std::string value{ seperator + 1, static_cast<std::size_t>(end - seperator - 1) };
		boost::algorithm::trim(value);
		self->_headers.insert({ std::move(key), std::move(value) });
	}

	// final
	if (size * count == 2) {
		// signal that all headers were received
		self->_data.lock()->status |= detail::headers_finished;
		if (self->_headers_received_handler) {
			CURLIO_TRACE("Signaling headers received");
			self->_headers_received_handler({});
			self->_headers_received_handler.reset();
		}
	}

	return size * count;
}

} // namespace curlio
