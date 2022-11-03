#pragma once

#include "basic_request.hpp"
#include "basic_response.hpp"
#include "basic_session.hpp"
#include "error.hpp"

namespace curlio {

template<typename Executor>
inline auto Basic_response<Executor>::async_read_some(const auto& buffers, auto&& token)
{
	return boost::asio::async_initiate<decltype(token), void(boost::system::error_code, std::size_t)>(
	  [this](auto handler, const auto& buffers) {
		  boost::asio::dispatch(_session->get_strand(), [this, buffers, handler = std::move(handler)]() mutable {
			  auto executor = boost::asio::get_associated_executor(handler, get_executor());

			  // Can immediately finish.
			  if (_input_buffer.size() > 0) {
				  const std::size_t copied = boost::asio::buffer_copy(buffers, _input_buffer.data());
				  _input_buffer.consume(copied);
				  boost::asio::post(std::move(executor),
				                    std::bind(std::move(handler), boost::system::error_code{}, copied));
			  } else if (_finished) {
				  boost::asio::post(std::move(executor),
				                    std::bind(std::move(handler),
				                              boost::system::error_code{ boost::asio::error::eof },
				                              std::size_t{ 0 }));
			  } else if (_receive_handler) {
				  boost::asio::post(
				    std::move(executor),
				    std::bind(std::move(handler), make_error_code(Code::multiple_reads), std::size_t{ 0 }));
			  } // Wait for more data.
			  else {
				  _receive_handler = [this, buffers = std::move(buffers), executor = std::move(executor),
				                      handler = std::move(handler)](boost::system::error_code ec, const char* data,
				                                                    std::size_t size) mutable {
					  const std::size_t copied = boost::asio::buffer_copy(buffers, boost::asio::buffer(data, size));
					  boost::asio::post(std::move(executor), std::bind(std::move(handler), ec, copied));
					  return copied;
				  };

				  // Resume.
				  curl_easy_pause(_request->native_handle(), CURLPAUSE_CONT);
			  }
		  });
	  },
	  token, buffers);
}

template<typename Executor>
inline auto Basic_response<Executor>::async_wait_headers(auto&& token)
{
	return boost::asio::async_initiate<decltype(token), void(boost::system::error_code)>(
	  [this](auto handler) {
		  boost::asio::dispatch(_session->get_strand(), [this, handler = std::move(handler)]() mutable {
			  _header_collector.async_wait(get_executor(), std::move(handler));
		  });
	  },
	  token);
}

template<typename Executor>
inline const Basic_response<Executor>::headers_type& Basic_response<Executor>::headers() const noexcept
{
	return _header_collector.fields();
}

template<typename Executor>
inline Basic_response<Executor>::executor_type Basic_response<Executor>::get_executor() const noexcept
{
	return _session->get_executor();
}

template<typename Executor>
inline Basic_response<Executor>::Basic_response(std::shared_ptr<Basic_session<Executor>>&& session,
                                                std::shared_ptr<Basic_request<Executor>>&& request)
    : _session{ std::move(session) }, _request{ std::move(request) }, _header_collector{
	      _request->native_handle()
      }
{
	curl_easy_setopt(_request->native_handle(), CURLOPT_WRITEFUNCTION, &Basic_response::_write_callback);
	curl_easy_setopt(_request->native_handle(), CURLOPT_WRITEDATA, this);
}

template<typename Executor>
inline void Basic_response<Executor>::_mark_finished()
{
	_finished = true;
	// _header_collector.finish();
}

template<typename Executor>
inline std::size_t Basic_response<Executor>::_write_callback(char* data, std::size_t size, std::size_t count,
                                                             void* self_ptr) noexcept
{
	const auto self                = static_cast<Basic_response*>(self_ptr);
	const std::size_t total_length = size * count;

	if (total_length == 0) {
		return 0;
	}

	// Someone is waiting for more data.
	if (self->_receive_handler) {
		const std::size_t immediately_consumed = self->_receive_handler({}, data, total_length);
		self->_receive_handler.reset();
		const std::size_t copied = boost::asio::buffer_copy(
		  self->_input_buffer.prepare(total_length - immediately_consumed),
		  boost::asio::buffer(data + immediately_consumed, total_length - immediately_consumed));
		self->_input_buffer.commit(copied);
		return immediately_consumed + copied;
	}

	return CURL_WRITEFUNC_PAUSE;
}

} // namespace curlio
