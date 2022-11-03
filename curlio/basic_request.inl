#pragma once

#include "basic_request.hpp"
#include "basic_session.hpp"

namespace curlio {

template<typename Executor>
inline Basic_request<Executor>::~Basic_request() noexcept
{
	curl_easy_cleanup(_handle);
}

template<typename Executor>
inline auto Basic_request<Executor>::async_write_some(const auto& buffers, auto&& token)
{
	return boost::asio::async_initiate<decltype(token), void(boost::system::error_code, std::size_t)>(
	  [this](auto handler, const auto& buffers) {
		  boost::asio::dispatch(_session->get_strand(), [this, buffers, handler = std::move(handler)]() mutable {
			  auto executor = boost::asio::get_associated_executor(handler, get_executor());

			  _send_handler = [this, buffers = std::move(buffers), handler = std::move(handler),
			                   executor = std::move(executor)](boost::system::error_code ec, char* data,
			                                                   std::size_t size) mutable {
				  const std::size_t copied = boost::asio::buffer_copy(boost::asio::buffer(data, size), buffers);
				  boost::asio::post(std::move(executor), std::bind(std::move(handler), ec, copied));
				  return copied;
			  };

			  // Resume.
			  curl_easy_pause(_handle, CURLPAUSE_CONT);
		  });
	  },
	  token, buffers);
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
