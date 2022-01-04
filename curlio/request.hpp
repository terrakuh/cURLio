#pragma once

#include "detail/function.hpp"
#include "detail/mover.hpp"
#include "error.hpp"

#include <boost/asio.hpp>
#include <cstdio>
#include <curl/curl.h>
#include <utility>

namespace curlio {

class Session;

class Request
{
public:
	/// This constructor assumes ownership of the provided handle.
	Request(CURL* handle = curl_easy_init()) noexcept;
	Request(Request&& move) = default;
	~Request() noexcept;

	void set_url(const char* url) { curl_easy_setopt(_handle, CURLOPT_URL, url); }
	bool is_valid() const noexcept { return _handle != nullptr; }
	CURL* native_handle() noexcept { return _handle; }
	/// Waits until the request is complete. Data must be read before this function.
	template<typename Token>
	auto async_wait(Token&& token);
	template<typename Mutable_buffer_sequence, typename Token>
	auto async_read_some(const Mutable_buffer_sequence& buffers, Token&& token);
	boost::asio::any_io_executor get_executor() noexcept { return _executor; }
	void swap(Request& other) noexcept;
	Request& operator=(Request&& move) = default;

private:
	friend Session;

	/// Set to the session's executor.
	boost::asio::any_io_executor _executor;
	/// This handler is set when an asynchronous action waits for data.
	detail::Function<void(boost::system::error_code)> _write_handler;
	int _pause_mask = 0;
	detail::Mover<CURL*> _handle;
	/// Stores one `_write_callback` call.
	boost::asio::streambuf _input_buffer;
	/// This handler is set when an asynchronous action waits for the request to complete.
	detail::Function<void()> _finish_handler;
	bool _finished = false;

	/// Marks this request as finished.
	void _finish();
	static std::size_t _write_callback(void* data, std::size_t size, std::size_t count,
	                                   void* self_pointer) noexcept;
};

inline Request::Request(CURL* handle) noexcept
{
	_handle = handle;
	if (_handle != nullptr) {
		curl_easy_setopt(_handle, CURLOPT_WRITEFUNCTION, &Request::_write_callback);
		curl_easy_setopt(_handle, CURLOPT_WRITEDATA, this);
		curl_easy_setopt(_handle, CURLOPT_PRIVATE, this);
	}
}

inline Request::~Request() noexcept
{
	if (_handle != nullptr) {
		curl_easy_cleanup(_handle);
	}
}

template<typename Token>
inline auto Request::async_wait(Token&& token)
{
	return boost::asio::async_initiate<Token, void(boost::system::error_code)>(
	  [this](auto handler) {
		  if (_finished) {
			  std::move(handler)(boost::system::error_code{});
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
	return boost::asio::async_initiate<Token, void(boost::system::error_code, std::size_t)>(
	  [this, buffers](auto handler) {
		  // can immediately finish
		  if (_input_buffer.size() > 0) {
			  const std::size_t copied = boost::asio::buffer_copy(buffers, _input_buffer.data());
			  _input_buffer.consume(copied);
			  std::move(handler)(boost::system::error_code{}, copied);
		  } else if (_write_handler) {
			  std::move(handler)(Code::multiple_reads, 0);
		  } else if (_finished) {
			  std::move(handler)(boost::asio::error::eof, 0);
		  } else {
			  // set write handler when cURL calls the write callback
			  _write_handler = [this, buffers, handler = std::move(handler)](boost::system::error_code ec) mutable {
				  std::size_t copied = 0;
				  // copy data and finish
				  if (!ec) {
					  copied = boost::asio::buffer_copy(buffers, _input_buffer.data());
					  _input_buffer.consume(copied);
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

inline void Request::_finish()
{
	_finished = true;
	if (_write_handler) {
		_write_handler(boost::asio::error::eof);
		_write_handler.reset();
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
	auto self = static_cast<Request*>(self_pointer);

	// data is wanted
	if (self->_write_handler) {
		const std::size_t copied = boost::asio::buffer_copy(self->_input_buffer.prepare(size * count),
		                                                    boost::asio::buffer(data, size * count));
		self->_input_buffer.commit(copied);
		self->_write_handler({});
		self->_write_handler.reset();
		return copied;
	}
	self->_pause_mask |= CURLPAUSE_RECV;
	return CURL_WRITEFUNC_PAUSE;
}

} // namespace curlio
