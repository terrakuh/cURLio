#pragma once

#include "basic_request.hpp"
#include "basic_response.hpp"
#include "basic_session.hpp"
#include "detail/socket_data.hpp"

#include <functional>

namespace curlio {

template<typename Executor>
inline Basic_session<Executor>::~Basic_session() noexcept
{
	_timer.cancel();

	curl_multi_cleanup(_multi_handle);
}

template<typename Executor>
inline auto Basic_session<Executor>::async_start(request_pointer request, auto&& token)
{
	return boost::asio::async_initiate<decltype(token),
	                                   void(boost::system::error_code, std::shared_ptr<Response>)>(
	  [this, request = std::move(request)](auto handler) mutable {
		  boost::asio::post(
		    _strand, [this, request = std::move(request), handler = std::move(handler)]() mutable {
			    const auto easy_handle = request->native_handle();
			    curl_multi_add_handle(_multi_handle, easy_handle);

			    // Kick start.
			    _perform(CURL_SOCKET_TIMEOUT, 0);

			    std::shared_ptr<Response> response{ new Response{ this->shared_from_this(), std::move(request) } };
			    _active_requests.insert({ easy_handle, response });

			    auto executor = boost::asio::get_associated_executor(handler, get_executor());
			    boost::asio::post(std::move(executor),
			                      std::bind(std::move(handler), boost::system::error_code{}, std::move(response)));
		    });
	  },
	  token);
}

template<typename Executor>
inline Basic_session<Executor>::executor_type Basic_session<Executor>::get_executor() const noexcept
{
	return _strand.get_inner_executor();
}

template<typename Executor>
inline boost::asio::strand<Executor>& Basic_session<Executor>::get_strand() noexcept
{
	return _strand;
}

template<typename Executor>
inline Basic_session<Executor>::Basic_session(Executor executor) : _strand{ std::move(executor) }
{
	_multi_handle = curl_multi_init();

	curl_multi_setopt(_multi_handle, CURLMOPT_SOCKETFUNCTION, &Basic_session::_socket_callback);
	curl_multi_setopt(_multi_handle, CURLMOPT_SOCKETDATA, this);

	curl_multi_setopt(_multi_handle, CURLMOPT_TIMERFUNCTION, &Basic_session::_timer_callback);
	curl_multi_setopt(_multi_handle, CURLMOPT_TIMERDATA, this);
}

template<typename Executor>
inline void Basic_session<Executor>::_clean_finished()
{
	CURLMsg* message = nullptr;
	int left         = 0;
	while ((message = curl_multi_info_read(_multi_handle, &left))) {
		if (message->msg == CURLMSG_DONE) {
			curl_multi_remove_handle(_multi_handle, message->easy_handle);

			const auto it = _active_requests.find(message->easy_handle);
			if (it != _active_requests.end()) {
				it->second->_mark_finished();
				_active_requests.erase(it);
			}
		}
	}
}

template<typename Executor>
inline void Basic_session<Executor>::_perform(curl_socket_t socket, int bitmask)
{
	int running = 0;
	curl_multi_socket_action(_multi_handle, socket, bitmask, &running);
	_clean_finished();
}

template<typename Executor>
inline int Basic_session<Executor>::_socket_callback(CURL* easy_handle, curl_socket_t socket, int what,
                                                     void* self_ptr, void* socket_data_ptr) noexcept
{
	const auto self  = static_cast<Basic_session*>(self_ptr)->shared_from_this();
	auto socket_data = static_cast<detail::Socket_data*>(socket_data_ptr);

	if (what == CURL_POLL_REMOVE) {
		delete socket_data;
		return CURLM_OK;
	} else if (socket_data == nullptr) {
		// TODO add family
		socket_data = new detail::Socket_data{ boost::asio::ip::tcp::socket{
			self->_strand, boost::asio::ip::tcp::v4(), socket } };
		curl_multi_assign(self->_multi_handle, socket, socket_data);
	}

	if (what == CURL_POLL_IN || what == CURL_POLL_INOUT) {
		socket_data->socket.async_wait(
		  boost::asio::socket_base::wait_read,
		  boost::asio::bind_cancellation_slot(
		    socket_data->cancel_wait_read.slot(),
		    [self, socket, socket_data](const auto& ec) {
			    if (!ec) {
				    socket_data->cancel_wait_write.emit(boost::asio::cancellation_type::total);
				    self->_perform(socket, CURL_CSELECT_IN);
			    }
		    }));
	}

	if (what == CURL_POLL_OUT || what == CURL_POLL_INOUT) {
		socket_data->socket.async_wait(
		  boost::asio::socket_base::wait_write,
		  boost::asio::bind_cancellation_slot(
		    socket_data->cancel_wait_write.slot(),
		    [self, socket, socket_data](const auto& ec) {
			    if (!ec) {
				    socket_data->cancel_wait_read.emit(boost::asio::cancellation_type::total);
				    self->_perform(socket, CURL_CSELECT_OUT);
			    }
		    }));
	}

	return CURLM_OK;
}

template<typename Executor>
inline int Basic_session<Executor>::_timer_callback(CURLM* multi_handle, long timeout_ms,
                                                    void* self_ptr) noexcept
{
	const auto self = static_cast<Basic_session*>(self_ptr)->shared_from_this();
	self->_timer.expires_after(std::chrono::milliseconds{ timeout_ms });
	self->_timer.async_wait([self](const auto& ec) {
		if (!ec) {
			self->_perform(CURL_SOCKET_TIMEOUT, 0);
		}
	});
	return 0;
}

template<typename Executor>
CURLIO_NO_DISCARD inline std::shared_ptr<Basic_session<Executor>> make_session(Executor executor)
{
	return std::shared_ptr<Basic_session<Executor>>{ new Basic_session<Executor>{ std::move(executor) } };
}

} // namespace curlio
