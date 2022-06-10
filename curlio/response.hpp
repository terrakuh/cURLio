#pragma once

#include "detail/function.hpp"
#include "detail/header_collector.hpp"
#include "detail/shared_data.hpp"
#include "error.hpp"
#include "log.hpp"

#include <algorithm>
#include <boost/asio.hpp>
#include <cctype>
#include <cstdio>
#include <curl/curl.h>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

namespace curlio {

class Session;

class Response {
public:
	typedef boost::asio::any_io_executor executor_type;

	Response(Response&& move) noexcept;
	~Response() noexcept;

	/// Returns the content length of the response or `-1` if unknown.
	curl_off_t content_length() noexcept;
	/// Returns the content type of the response. The string will get freed when `this` dies.
	std::string_view content_type() noexcept;
	long response_code();
	bool is_redirect() const noexcept
	{
		return _header_collector.fields().find("location") != _header_collector.fields().end();
	}
	bool is_valid() const noexcept { return !_data.expired(); }
	bool is_active() const noexcept { return is_valid(); }
	CURL* native_handle() noexcept;
	detail::Header_collector::Fields& header_fields() noexcept { return _header_collector.fields(); }
	template<typename Token>
	auto async_await_next_headers(Token&& token)
	{
		return _header_collector.async_await_headers(std::forward<Token>(token));
	}
	template<typename Token>
	auto async_await_last_headers(Token&& token);
	/// Waits until the response is complete. Data must be read before this function.
	template<typename Token>
	auto async_await_completion(Token&& token);
	template<typename Mutable_buffer_sequence, typename Token>
	auto async_read_some(const Mutable_buffer_sequence& buffers, Token&& token);
	/// Getting the executor of an invalid response is undefined behavior.
	boost::asio::any_io_executor get_executor() noexcept { return _header_collector.get_executor(); }
	Response& operator=(Response&& move) noexcept;

private:
	friend Session;

	std::weak_ptr<detail::Shared_data> _data;
	detail::Header_collector _header_collector;
	/// This handler is set when an asynchronous action waits for data.
	detail::Function<void(boost::system::error_code)> _receive_handler;
	detail::Function<void()> _finish_handler;
	/// This buffer is required when more data is received than read.
	boost::asio::streambuf _input_buffer;

	Response(const std::shared_ptr<detail::Shared_data>& data) noexcept;
	static std::size_t _receive_callback(void* data, std::size_t size, std::size_t count,
	                                     void* self_pointer) noexcept;
};

inline Response::Response(Response&& move) noexcept : Response{ move._data.lock() }
{
	move._data.reset();
	std::swap(_header_collector, move._header_collector);
	std::swap(_receive_handler, move._receive_handler);
	std::swap(_finish_handler, move._finish_handler);

	const auto buffer = move._input_buffer.data();
	if (buffer.size() != 0) {
		const auto copied = boost::asio::buffer_copy(_input_buffer.prepare(buffer.size()), buffer);
		_input_buffer.commit(copied);
		move._input_buffer.consume(copied);
	}
}

inline Response::~Response() noexcept
{
	const auto handle = native_handle();
	if (handle != nullptr) {
		curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, nullptr);
		curl_easy_setopt(handle, CURLOPT_WRITEDATA, nullptr);
		_header_collector.unhook(handle);
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

inline long Response::response_code()
{
	long response   = 0;
	const auto code = curl_easy_getinfo(native_handle(), CURLINFO_RESPONSE_CODE, &response);
	if (code != CURLE_OK) {
		throw boost::system::system_error{ Code::no_response_code, curl_easy_strerror(code) };
	}
	return response;
}

inline CURL* Response::native_handle() noexcept
{
	const auto ptr = _data.lock();
	return ptr == nullptr ? nullptr : ptr->handle;
}

template<typename Token>
inline auto Response::async_await_last_headers(Token&& token)
{
	return boost::asio::async_compose<Token, void(boost::system::error_code)>(
	  [this, started = false](auto& self, boost::system::error_code ec = {}) mutable {
		  if (ec || (started && !is_redirect())) {
			  self.complete(ec);
		  } else {
			  started = true;
			  async_await_next_headers(std::move(self));
		  }
	  },
	  token, get_executor());
}

template<typename Token>
inline auto Response::async_await_completion(Token&& token)
{
	return boost::asio::async_initiate<Token, void(boost::system::error_code)>(
	  [this](auto&& handler) {
		  const auto ptr = _data.lock();
		  auto executor  = boost::asio::get_associated_executor(handler, ptr->executor);

		  if (ptr == nullptr || ptr->status & detail::finished) {
			  boost::asio::post(executor, std::bind(std::move(handler), boost::system::error_code{}));
		  } else if (_finish_handler) {
			  boost::asio::post(
			    executor, std::bind(std::move(handler), make_error_code(Code::multiple_completion_awaitings)));
		  } else {
			  _finish_handler = [handler = std::move(handler), executor]() mutable {
				  boost::asio::post(executor, std::bind(std::move(handler), boost::system::error_code{}));
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
	  [this, buffers](auto&& handler) {
		  const auto ptr = _data.lock();
		  auto executor  = boost::asio::get_associated_executor(handler, ptr->executor);

		  // can immediately finish
		  if (_input_buffer.size() > 0) {
			  const std::size_t copied = boost::asio::buffer_copy(buffers, _input_buffer.data());
			  _input_buffer.consume(copied);
			  boost::asio::post(executor, std::bind(std::move(handler), boost::system::error_code{}, copied));
		  } else if (_receive_handler) {
			  boost::asio::post(executor, std::bind(std::move(handler), make_error_code(Code::multiple_reads), 0));
		  } else if (ptr == nullptr || ptr->status & detail::finished) {
			  boost::asio::post(executor,
			                    std::bind(std::move(handler), make_error_code(boost::asio::error::eof), 0));
		  } else {
			  // set write handler when cURL calls the write callback
			  _receive_handler = [this, buffers, executor,
			                      handler = std::move(handler)](boost::system::error_code ec) mutable {
				  std::size_t copied = 0;
				  // copy data and finish
				  if (!ec) {
					  copied = boost::asio::buffer_copy(buffers, _input_buffer.data());
					  _input_buffer.consume(copied);
					  CURLIO_DEBUG("Read " << copied << " bytes");
				  }
				  boost::asio::post(executor, std::bind(std::move(handler), ec, copied));
			  };

			  // TODO check for errors
			  boost::asio::dispatch(ptr->executor, [ptr] {
				  if ((ptr->pause_mask & CURLPAUSE_RECV) == CURLPAUSE_RECV) {
					  ptr->pause_mask &= ~CURLPAUSE_RECV;
					  curl_easy_pause(ptr->handle, ptr->pause_mask);
				  }
			  });
		  }
	  },
	  token);
}

inline Response& Response::operator=(Response&& move) noexcept
{
	this->~Response();
	new (this) Response{ std::move(move) };
	return *this;
}

inline Response::Response(const std::shared_ptr<detail::Shared_data>& data) noexcept
    : _data{ data }, _header_collector{ data->executor }
{
	if (data != nullptr) {
		curl_easy_setopt(data->handle, CURLOPT_WRITEFUNCTION, &Response::_receive_callback);
		curl_easy_setopt(data->handle, CURLOPT_WRITEDATA, this);
		_header_collector.hook(data->handle);
		data->response_finisher = [this] {
			CURLIO_TRACE("Response " << this << " finished");
			_header_collector.finish();
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

} // namespace curlio
