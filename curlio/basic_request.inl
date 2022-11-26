#pragma once

#include "basic_request.hpp"
#include "basic_session.hpp"
#include "error.hpp"

namespace curlio {

template<typename Executor>
inline Basic_request<Executor>::Basic_request(const Basic_request& copy) : _session{ copy._session }
{
	_handle = curl_easy_duphandle(copy._handle);
}

template<typename Executor>
inline Basic_request<Executor>::~Basic_request() noexcept
{
	curl_easy_cleanup(_handle);
	curl_slist_free_all(_additional_headers);
}

template<typename Executor>
template<CURLoption Option>
inline void Basic_request<Executor>::set_option(detail::option_type<Option> value)
{
	if (const auto status = curl_easy_setopt(_handle, Option, value); status != CURLE_OK) {
		throw std::system_error{ Code::bad_option, curl_easy_strerror(status) };
	}
}

template<typename Executor>
inline void Basic_request<Executor>::append_header(const char* header)
{
	_additional_headers = curl_slist_append(_additional_headers, header);
	set_option<CURLOPT_HTTPHEADER>(_additional_headers);
}

template<typename Executor>
inline auto Basic_request<Executor>::async_write_some(const auto& buffers, auto&& token)
{
	return CURLIO_ASIO_NS::async_initiate<decltype(token), void(detail::asio_error_code, std::size_t)>(
	  [this](auto handler, const auto& buffers) {
		  CURLIO_ASIO_NS::dispatch(
		    _session->get_strand(), [this, buffers, handler = std::move(handler)]() mutable {
			    auto executor = CURLIO_ASIO_NS::get_associated_executor(handler, get_executor());

			    if (_send_handler) {
				    CURLIO_ASIO_NS::post(
				      std::move(executor),
				      std::bind(std::move(handler), make_error_code(Code::multiple_writes), std::size_t{ 0 }));
			    } else {
				    _send_handler = [this, buffers = std::move(buffers), handler = std::move(handler),
				                     executor = std::move(executor)](detail::asio_error_code ec, char* data,
				                                                     std::size_t size) mutable {
					    const std::size_t copied =
					      CURLIO_ASIO_NS::buffer_copy(CURLIO_ASIO_NS::buffer(data, size), buffers);
					    CURLIO_ASIO_NS::post(std::move(executor), std::bind(std::move(handler), ec, copied));
					    return copied;
				    };

				    // Resume.
				    curl_easy_pause(_handle, CURLPAUSE_CONT);
			    }
		    });
	  },
	  token, buffers);
}

template<typename Executor>
inline auto Basic_request<Executor>::async_abort(auto&& token)
{
	return CURLIO_ASIO_NS::async_initiate<decltype(token), void(detail::asio_error_code)>(
	  [this](auto handler) {
		  CURLIO_ASIO_NS::dispatch(_session->get_strand(), [this, handler = std::move(handler)]() mutable {
			  if (_send_handler) {
				  _send_handler(CURLIO_ASIO_NS::error::operation_aborted, nullptr, 0);
				  _send_handler.reset();
			  }

			  auto executor = CURLIO_ASIO_NS::get_associated_executor(handler, get_executor());

			  _send_handler = [this, handler = std::move(handler), executor = std::move(executor)](
			                    detail::asio_error_code ec, char*, std::size_t) mutable {
				  CURLIO_ASIO_NS::post(std::move(executor), std::bind(std::move(handler), ec, std::size_t{ 0 }));
				  return CURL_READFUNC_ABORT;
			  };

			  // Resume.
			  curl_easy_pause(_handle, CURLPAUSE_CONT);
		  });
	  },
	  token);
}

template<typename Executor>
inline CURL* Basic_request<Executor>::native_handle() const noexcept
{
	return _handle;
}

template<typename Executor>
inline Basic_request<Executor>::executor_type Basic_request<Executor>::get_executor() const noexcept
{
	return _session->get_executor();
}

template<typename Executor>
inline Basic_request<Executor>::Basic_request(std::shared_ptr<Basic_session<Executor>>&& session)
    : _session{ std::move(session) }
{
	_handle = curl_easy_init();
}

template<typename Executor>
inline void Basic_request<Executor>::_mark_finished()
{
	if (_send_handler) {
		_send_handler(CURLIO_ASIO_NS::error::eof, nullptr, 0);
		_send_handler.reset();
	}
}

template<typename Executor>
inline std::size_t Basic_request<Executor>::_read_callback(char* data, std::size_t size, std::size_t count,
                                                           void* self_ptr) noexcept
{
	const auto self                = static_cast<Basic_request*>(self_ptr);
	const std::size_t total_length = size * count;

	if (total_length == 0) {
		return 0;
	}

	// Someone is waiting for more data.
	if (self->_send_handler) {
		const std::size_t bytes_transferred = self->_send_handler({}, data, total_length);
		self->_send_handler.reset();
		return bytes_transferred;
	}

	return CURL_READFUNC_PAUSE;
}

template<typename Executor>
CURLIO_NO_DISCARD inline std::shared_ptr<Basic_request<Executor>>
  make_request(std::shared_ptr<Basic_session<Executor>> session)
{
	return std::shared_ptr<Basic_request<Executor>>{ new Basic_request<Executor>{ std::move(session) } };
}

} // namespace curlio
