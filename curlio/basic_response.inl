#pragma once

#include "basic_request.hpp"
#include "basic_response.hpp"
#include "basic_session.hpp"
#include "error.hpp"
#include "log.hpp"

namespace curlio {

template<typename Executor>
template<CURLINFO Option>
inline auto Basic_response<Executor>::async_get_info(auto&& token) const
{
	return CURLIO_ASIO_NS::async_initiate<decltype(token),
	                                      void(detail::asio_error_code, detail::info_type<Option>)>(
	  [this](auto handler) {
		  CURLIO_ASIO_NS::dispatch(_session->get_strand(), [this, handler = std::move(handler)]() mutable {
			  detail::info_type<Option> value{};
			  detail::asio_error_code ec{};

			  // TODO
			  curl_easy_getinfo(_request->native_handle(), Option, &value);

			  auto executor = CURLIO_ASIO_NS::get_associated_executor(handler, get_executor());
			  CURLIO_ASIO_NS::post(std::move(executor), std::bind(std::move(handler), ec, value));
		  });
	  },
	  token);
}

template<typename Executor>
inline auto Basic_response<Executor>::async_read_some(const auto& buffers, auto&& token)
{
	return CURLIO_ASIO_NS::async_initiate<decltype(token), void(detail::asio_error_code, std::size_t)>(
	  [this](auto handler, const auto& buffers) {
		  CURLIO_ASIO_NS::dispatch(
		    _session->get_strand(), [this, buffers, handler = std::move(handler)]() mutable {
			    auto executor = CURLIO_ASIO_NS::get_associated_executor(handler, get_executor());
			    // Can immediately finish.
			    if (_input_buffer.size() > 0) {
				    const std::size_t copied = CURLIO_ASIO_NS::buffer_copy(buffers, _input_buffer.data());
				    _input_buffer.consume(copied);
				    CURLIO_ASIO_NS::post(std::move(executor),
				                         std::bind(std::move(handler), detail::asio_error_code{}, copied));
			    } else if (_finished) {
				    CURLIO_ASIO_NS::post(std::move(executor),
				                         std::bind(std::move(handler),
				                                   detail::asio_error_code{ CURLIO_ASIO_NS::error::eof },
				                                   std::size_t{ 0 }));
			    } else if (_receive_handler) {
				    CURLIO_ASIO_NS::post(
				      std::move(executor),
				      std::bind(std::move(handler), make_error_code(Code::multiple_reads), std::size_t{ 0 }));
			    } // Wait for more data.
			    else {
				    _receive_handler = [this, buffers = std::move(buffers), executor = std::move(executor),
				                        handler = std::move(handler)](detail::asio_error_code ec, const char* data,
				                                                      std::size_t size) mutable {
					    const std::size_t copied =
					      CURLIO_ASIO_NS::buffer_copy(buffers, CURLIO_ASIO_NS::buffer(data, size));
					    CURLIO_ASIO_NS::post(std::move(executor), std::bind(std::move(handler), ec, copied));
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
	return CURLIO_ASIO_NS::async_initiate<decltype(token), void(detail::asio_error_code, headers_type)>(
	  [this](auto handler) {
		  CURLIO_ASIO_NS::dispatch(_session->get_strand(), [this, handler = std::move(handler)]() mutable {
			  _header_collector.async_wait(get_executor(), std::move(handler));
		  });
	  },
	  token);
}

template<typename Executor>
inline Basic_response<Executor>::executor_type Basic_response<Executor>::get_executor() const noexcept
{
	return _session->get_executor();
}

template<typename Executor>
inline Basic_response<Executor>::Basic_response(std::shared_ptr<Basic_session<Executor>> session,
                                                std::shared_ptr<Basic_request<Executor>> request)
    : _session{ std::move(session) }, _request{ std::move(request) },
      _header_collector{ _request->native_handle() }
{}

template<typename Executor>
inline void Basic_response<Executor>::_start() noexcept
{
	curl_easy_setopt(_request->native_handle(), CURLOPT_WRITEFUNCTION, &Basic_response::_write_callback);
	curl_easy_setopt(_request->native_handle(), CURLOPT_WRITEDATA, this);
	_header_collector.start();
}

template<typename Executor>
inline void Basic_response<Executor>::_stop() noexcept
{
	CURLIO_INFO("Response marked as finished");

	_header_collector.stop();
	curl_easy_setopt(_request->native_handle(), CURLOPT_WRITEFUNCTION, nullptr);
	curl_easy_setopt(_request->native_handle(), CURLOPT_WRITEDATA, nullptr);

	_finished = true;
	if (_receive_handler) {
		_receive_handler(CURLIO_ASIO_NS::error::eof, nullptr, 0);
		_receive_handler.reset();
	}

	_request->_mark_finished();
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
		CURLIO_TRACE("Received " << total_length << " bytes and consumed " << immediately_consumed);

		const std::size_t copied = CURLIO_ASIO_NS::buffer_copy(
		  self->_input_buffer.prepare(total_length - immediately_consumed),
		  CURLIO_ASIO_NS::buffer(data + immediately_consumed, total_length - immediately_consumed));
		self->_input_buffer.commit(copied);
		return immediately_consumed + copied;
	}

	CURLIO_TRACE("Received " << total_length << " bytes but pausing");
	return CURL_WRITEFUNC_PAUSE;
}

template<typename Executor>
inline auto async_wait_last_headers(std::shared_ptr<Basic_response<Executor>> response, auto&& token)
{
	using headers_type = Basic_response<Executor>::headers_type;
	return CURLIO_ASIO_NS::async_compose<decltype(token), void(detail::asio_error_code, headers_type)>(
	  [response = std::move(response), started = false](auto& self, detail::asio_error_code ec = {},
	                                                    headers_type headers = {}) mutable {
		  if (!started || (!ec && headers.count("location") > 0)) {
			  started = true;
			  response->async_wait_headers(std::move(self));
		  } else {
			  self.complete(ec, std::move(headers));
		  }
	  },
	  token);
}

} // namespace curlio
