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
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace curlio {

class Session;

class Request
{
public:
	typedef boost::asio::any_io_executor executor_type;

	Request() = default;
	Request(Request&& move) noexcept;
	~Request() noexcept;

	void set_url(const char* url);
	void append_http_field(const char* field);
	void set_method(const char* method);
	void set_content_length(curl_off_t length);
	bool is_valid() const noexcept { return !_data.expired(); }
	bool is_active() const noexcept { return is_valid() && _owner == nullptr; }
	CURL* native_handle() noexcept;
	template<typename Const_buffer_sequence, typename Token>
	auto async_write_some(const Const_buffer_sequence& buffers, Token&& token);
	/// Getting the executor of an inactive request is undefined behavior.
	boost::asio::any_io_executor get_executor() noexcept { return _data.lock()->executor; }
	Request& operator=(Request&& move) noexcept;

private:
	friend Session;

	/// The owner of a started request is the session which sets this to `nullptr`.
	std::shared_ptr<detail::Shared_data> _owner;
	std::weak_ptr<detail::Shared_data> _data;
	curl_slist* _request_headers = nullptr;
	detail::Function<std::size_t(boost::system::error_code, void*, std::size_t)> _send_handler;

	void _lazy_init();
	void _hook(detail::Shared_data& data) noexcept;
	static std::size_t _send_callback(void* data, std::size_t size, std::size_t count,
	                                  void* self_pointer) noexcept;
};

inline Request::Request(Request&& move) noexcept
{
	std::swap(_owner, move._owner);
	std::swap(_data, move._data);
	std::swap(_request_headers, move._request_headers);
	std::swap(_send_handler, move._send_handler);

	if (_owner != nullptr) {
		_hook(*_owner);
	} else {
		const auto data = _data.lock();
		if (data != nullptr) {
			_hook(*data);
		}
	}
}

inline Request::~Request() noexcept { curl_slist_free_all(_request_headers); }

inline void Request::set_url(const char* url)
{
	_lazy_init();
	if (curl_easy_setopt(native_handle(), CURLOPT_URL, url) != CURLE_OK) {
		throw std::system_error{ Code::bad_url };
	}
}

inline void Request::append_http_field(const char* field)
{
	_lazy_init();
	_request_headers = curl_slist_append(_request_headers, field);
	curl_easy_setopt(native_handle(), CURLOPT_HTTPHEADER, _request_headers);
}

inline void Request::set_method(const char* method)
{
	_lazy_init();
	std::string_view tmp = method;
	if (boost::algorithm::iequals(tmp, "get")) {
		curl_easy_setopt(native_handle(), CURLOPT_HTTPGET, 1);
	} else if (boost::algorithm::iequals(tmp, "post")) {
		curl_easy_setopt(native_handle(), CURLOPT_POST, 1);
	} else {
		curl_easy_setopt(native_handle(), CURLOPT_CUSTOMREQUEST, method);
	}
}

inline void Request::set_content_length(curl_off_t length)
{
	_lazy_init();
	curl_easy_setopt(native_handle(), CURLOPT_POSTFIELDSIZE_LARGE, length);
}

inline CURL* Request::native_handle() noexcept
{
	if (_owner != nullptr) {
		return _owner->handle;
	}
	const auto ptr = _data.lock();
	return ptr == nullptr ? nullptr : ptr->handle;
}

template<typename Const_buffer_sequence, typename Token>
inline auto Request::async_write_some(const Const_buffer_sequence& buffers, Token&& token)
{
	return boost::asio::async_initiate<Token, void(boost::system::error_code, std::size_t)>(
	  [this, buffers](auto handler) {
		  const auto ptr = _data.lock();
		  auto executor  = boost::asio::get_associated_executor(handler, ptr->executor);

		  // can immediately finish
		  if (ptr == nullptr || ptr->status & detail::finished) {
			  boost::asio::post(executor,
			                    [handler = std::move(handler)]() mutable { handler(boost::asio::error::eof, 0); });
		  } else if (_send_handler) {
			  boost::asio::post(executor,
			                    [handler = std::move(handler)]() mutable { handler(Code::multiple_writes, 0); });
		  } else {
			  // set write handler when cURL calls the write callback
			  _send_handler = [this, buffers, handler = std::move(handler), executor = ptr->executor](
			                    boost::system::error_code ec, void* data, std::size_t size) mutable {
				  // copy data and finish
				  const std::size_t copied = boost::asio::buffer_copy(boost::asio::buffer(data, size), buffers);
				  boost::asio::post(executor, [handler = std::move(handler), ec, copied]() mutable {
					  std::move(handler)(ec, copied);
				  });
				  return copied;
			  };

			  // TODO check for errors
			  ptr->pause_mask &= ~CURLPAUSE_SEND;
			  curl_easy_pause(ptr->handle, ptr->pause_mask);
		  }
	  },
	  token);
}

inline Request& Request::operator=(Request&& move) noexcept
{
	this->~Request();
	new (this) Request{ std::move(move) };
	return *this;
}

inline void Request::_lazy_init()
{
	if (!_data.expired()) {
		return;
	}
	_owner = std::make_shared<detail::Shared_data>();
	_data  = _owner;
	_hook(*_owner);
}

inline void Request::_hook(detail::Shared_data& data) noexcept
{
	curl_easy_setopt(data.handle, CURLOPT_READFUNCTION, &Request::_send_callback);
	curl_easy_setopt(data.handle, CURLOPT_READDATA, this);
	// enable all encodings
	curl_easy_setopt(data.handle, CURLOPT_ACCEPT_ENCODING, "");
	data.request_finisher = [this] {
		if (_send_handler) {
			_send_handler(boost::asio::error::eof, nullptr, 0);
			_send_handler.reset();
		}
	};
}

inline std::size_t Request::_send_callback(void* data, std::size_t size, std::size_t count,
                                           void* self_pointer) noexcept
{
	CURLIO_TRACE("Sending callback with buffer size of " << size * count << " bytes");
	const auto self = static_cast<Request*>(self_pointer);

	if (self->_send_handler) {
		const std::size_t copied = self->_send_handler({}, data, size * count);
		self->_send_handler.reset();
		if (copied > 0) {
			CURLIO_DEBUG("Sending " << copied << " bytes");
			return copied;
		}
	}

	// nobody wants to send -> pause for now
	CURLIO_TRACE("Pausing sending function");
	self->_data.lock()->pause_mask |= CURLPAUSE_SEND;
	return CURL_READFUNC_PAUSE;
}

} // namespace curlio
