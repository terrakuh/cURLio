#pragma once

#include "basic_response.hpp"
#include "basic_session.hpp"
#include "debug.hpp"
#include "detail/final_action.hpp"

#include <functional>
#include <optional>

namespace cURLio {

template<typename Executor>
inline BasicSession<Executor>::BasicSession(Executor executor)
    : _strand{ std::make_shared<strand_type>(CURLIO_ASIO_NS::make_strand(std::move(executor))) }
{
	_multi_handle = curl_multi_init();

	CURLIO_MULTI_ASSERT(
	  curl_multi_setopt(_multi_handle, CURLMOPT_SOCKETFUNCTION, &BasicSession::_socket_callback));
	CURLIO_MULTI_ASSERT(curl_multi_setopt(_multi_handle, CURLMOPT_SOCKETDATA, this));

	CURLIO_MULTI_ASSERT(
	  curl_multi_setopt(_multi_handle, CURLMOPT_TIMERFUNCTION, &BasicSession::_timer_callback));
	CURLIO_MULTI_ASSERT(curl_multi_setopt(_multi_handle, CURLMOPT_TIMERDATA, this));
}

template<typename Executor>
inline BasicSession<Executor>::~BasicSession()
{
	_timer.cancel();

	// TODO remove all active easy handles

	CURLIO_MULTI_CHECK(curl_multi_cleanup(_multi_handle));
}

template<typename Executor>
inline auto BasicSession<Executor>::async_start(request_pointer request, auto&& token)
{
	return CURLIO_ASIO_NS::async_initiate<decltype(token),
	                                      void(detail::asio_error_code, std::shared_ptr<Response>)>(
	  [this, request = std::move(request)](auto handler) mutable {
		  CURLIO_ASIO_NS::dispatch(
		    *_strand, [this, request = std::move(request), handler = std::move(handler)]() mutable {
			    // If the handle was already registered but the start was too fast, we need to clean it first.
			    _perform(CURL_SOCKET_TIMEOUT, 0);

			    const auto easy_handle = request->native_handle();

			    // TODO error
			    request->template set_option<CURLOPT_OPENSOCKETFUNCTION>(&BasicSession::_open_socket_callback);
			    request->template set_option<CURLOPT_OPENSOCKETDATA>(this);
			    request->template set_option<CURLOPT_CLOSESOCKETFUNCTION>(&BasicSession::_close_socket_callback);
			    request->template set_option<CURLOPT_CLOSESOCKETDATA>(this);
			    auto unregister_request = detail::finally([&] {
				    request->template set_option<CURLOPT_OPENSOCKETFUNCTION>(nullptr);
				    request->template set_option<CURLOPT_OPENSOCKETDATA>(nullptr);
				    request->template set_option<CURLOPT_CLOSESOCKETFUNCTION>(nullptr);
				    request->template set_option<CURLOPT_CLOSESOCKETDATA>(nullptr);
			    });

			    // Kick start.
			    CURLIO_TRACE("Kick-starting handle @" << easy_handle);
			    auto executor = CURLIO_ASIO_NS::get_associated_executor(handler, get_executor());
			    if (const auto err = CURLIO_MULTI_CHECK(curl_multi_add_handle(_multi_handle, easy_handle)); err) {
				    CURLIO_ASIO_NS::post(std::move(executor),
				                         std::bind(std::move(handler), err, std::shared_ptr<Response>{}));
				    return;
			    }
			    CURLIO_ASIO_NS::post(*_strand, [this] { _perform(CURL_SOCKET_TIMEOUT, 0); });

			    const std::shared_ptr<Response> response{ new Response{ _strand, request } };
			    if (const auto err = response->_start(); err) {
				    CURLIO_ASIO_NS::post(std::move(executor),
				                         std::bind(std::move(handler), err, std::shared_ptr<Response>{}));
				    return;
			    }
			    auto unregister_response = detail::finally([&] {
				    CURLIO_ERROR("Something prevented lift-off @" << easy_handle);
				    static_cast<void>(response->_stop());
				    _active_requests.erase(easy_handle);
			    });
			    _active_requests.insert({ easy_handle, response });

			    CURLIO_ASIO_NS::post(std::move(executor),
			                         std::bind(std::move(handler), detail::asio_error_code{}, response));

			    // Everything went without exceptions.
			    unregister_response.cancel();
			    unregister_request.cancel();
		    });
	  },
	  token);
}

template<typename Executor>
inline typename BasicSession<Executor>::executor_type BasicSession<Executor>::get_executor() const noexcept
{
	return _strand->get_inner_executor();
}

template<typename Executor>
inline typename BasicSession<Executor>::strand_type& BasicSession<Executor>::get_strand() noexcept
{
	return *_strand;
}

template<typename Executor>
inline void BasicSession<Executor>::_monitor(const std::shared_ptr<detail::SocketData>& data,
                                             detail::SocketData::WaitFlag type) noexcept
{
	CURLIO_TRACE("Monitoring on socket #" << data->socket.native_handle() << " flags=" << data->wait_flags
	                                      << " type=" << static_cast<int>(type));
	if (data->wait_flags & type) {
		data->socket.async_wait(
		  type == detail::SocketData::wait_flag_write ? CURLIO_ASIO_NS::socket_base::wait_write
		                                              : CURLIO_ASIO_NS::socket_base::wait_read,
		  [this, type, data](const detail::asio_error_code& ec) {
			  CURLIO_TRACE("Socket #" << data->socket.native_handle()
			                          << " action occurred (flags=" << data->wait_flags << "): " << ec.what());
			  if (!ec && data->wait_flags & type) {
				  _perform(data->socket.native_handle(),
				           type == detail::SocketData::wait_flag_write ? CURL_CSELECT_OUT : CURL_CSELECT_IN);
				  _monitor(data, type);
			  }
		  });
	}
}

template<typename Executor>
inline void BasicSession<Executor>::_clean_finished() noexcept
{
	const auto unregister = [&](typename decltype(_active_requests)::iterator it) {
		CURLIO_ASSERT(it != _active_requests.end());

		CURLIO_MULTI_CHECK(curl_multi_remove_handle(_multi_handle, it->first));

		CURLIO_EASY_CHECK(curl_easy_setopt(it->first, CURLOPT_OPENSOCKETFUNCTION, nullptr));
		CURLIO_EASY_CHECK(curl_easy_setopt(it->first, CURLOPT_OPENSOCKETDATA, nullptr));
		CURLIO_EASY_CHECK(curl_easy_setopt(it->first, CURLOPT_CLOSESOCKETFUNCTION, nullptr));
		CURLIO_EASY_CHECK(curl_easy_setopt(it->first, CURLOPT_CLOSESOCKETDATA, nullptr));

		static_cast<void>(it->second->_stop());
		return _active_requests.erase(it);
	};

	const std::size_t active_count = _active_requests.size();
	CURLMsg* message               = nullptr;
	int left                       = 0;
	while ((message = curl_multi_info_read(_multi_handle, &left))) {
		if (message->msg == CURLMSG_DONE) {
			CURLIO_INFO("Removing handle @" << message->easy_handle);
			unregister(_active_requests.find(message->easy_handle));
		} else {
			CURLIO_WARN("Got unknown message '" << message->msg << "' during cleaning for @"
			                                    << message->easy_handle);
		}
	}

	// Unregister all active request that have no other owner.
	for (auto it = _active_requests.begin(); it != _active_requests.end();) {
		if (it->second.use_count() == 1) {
			CURLIO_INFO("Stale handle @" << it->first << " found in active requests");
			it = unregister(it);
		} else {
			++it;
		}
	}

	if (active_count != _active_requests.size()) {
		CURLIO_INFO("Cleaned " << active_count - _active_requests.size() << " handles and left with "
		                       << _active_requests.size() << " active ones");
	}
}

template<typename Executor>
inline void BasicSession<Executor>::_perform(curl_socket_t socket, int bitmask) noexcept
{
	int running = 0;
	CURLIO_MULTI_CHECK(curl_multi_socket_action(_multi_handle, socket, bitmask, &running));
	CURLIO_TRACE("Action performed for socket #" << socket << " with bitmask " << bitmask
	                                             << " and running handles " << running);
	_clean_finished();
}

template<typename Executor>
inline int BasicSession<Executor>::_socket_callback(CURL* easy_handle, curl_socket_t socket, int what,
                                                    void* self_ptr, void* socket_data_ptr) noexcept
{
	constexpr const char* what_names[] = { "IN", "OUT", "IN/OUT", "REMOVE" };
	CURLIO_TRACE("Action callback on socket #" << socket << " in handle @" << easy_handle << ": "
	                                           << (what >= 1 && what <= 4 ? what_names[what - 1] : "unknown"));

	const auto self = static_cast<BasicSession*>(self_ptr);

	const auto it = self->_sockets.find(socket);
	if (it == self->_sockets.end()) {
		CURLIO_WARN("Socket #" << socket << " in handle @" << easy_handle << " was not found");
		return CURLM_OK;
	}

	const auto& data = it->second;
	data->wait_flags = 0;

	if (what == CURL_POLL_REMOVE) {
		data->socket.cancel();
		return CURLM_OK;
	}

	if (what == CURL_POLL_IN || what == CURL_POLL_INOUT) {
		data->wait_flags |= detail::SocketData::wait_flag_read;
		self->_monitor(data, detail::SocketData::wait_flag_read);
	}
	if (what == CURL_POLL_OUT || what == CURL_POLL_INOUT) {
		data->wait_flags |= detail::SocketData::wait_flag_write;
		self->_monitor(data, detail::SocketData::wait_flag_write);
	}

	return CURLM_OK;
}

template<typename Executor>
inline int BasicSession<Executor>::_timer_callback(CURLM* multi_handle, long timeout_ms,
                                                   void* self_ptr) noexcept
{
	const auto self = static_cast<BasicSession*>(self_ptr);
	if (timeout_ms == -1) {
		CURLIO_DEBUG("Removing timer");
		self->_timer.cancel();
	} else {
		if (timeout_ms == 0) {
			CURLIO_DEBUG("Adding immediate timer");
		} else {
			CURLIO_DEBUG("Adding timer with timeout of " << timeout_ms << " ms");
		}

		self->_timer.expires_after(std::chrono::milliseconds{ timeout_ms });
		self->_timer.async_wait([self](const detail::asio_error_code& ec) {
			CURLIO_DEBUG("Timeout occurred ec=" << ec.message());
			if (!ec) {
				self->_perform(CURL_SOCKET_TIMEOUT, 0);
			}
		});
	}

	return 0;
}

template<typename Executor>
inline curl_socket_t BasicSession<Executor>::_open_socket_callback(void* self_ptr, curlsocktype purpose,
                                                                   curl_sockaddr* address) noexcept
{
	CURLIO_TRACE("Trying to open new socket with family=" << address->family << " purpose=" << purpose);
	const auto self = static_cast<BasicSession*>(self_ptr);

	std::optional<CURLIO_ASIO_NS::ip::tcp> protocol{};
	if (address->family == AF_INET) {
		protocol = CURLIO_ASIO_NS::ip::tcp::v4();
	} else if (address->family == AF_INET6) {
		protocol = CURLIO_ASIO_NS::ip::tcp::v6();
	}

	if (protocol.has_value()) {
		detail::asio_error_code ec{};
		auto data = std::make_shared<detail::SocketData>(CURLIO_ASIO_NS::ip::tcp::socket{ self->get_strand() });
		static_cast<void>(data->socket.open(protocol.value(), ec));
		if (!ec) {
			const auto fd = data->socket.native_handle();
			CURLIO_INFO("New socket #" << fd << " opened");
			self->_sockets.insert({ fd, std::move(data) });
			return fd;
		}
	}

	CURLIO_ERROR("Failed to open socket");
	return CURL_SOCKET_BAD;
}

template<typename Executor>
inline int BasicSession<Executor>::_close_socket_callback(void* self_ptr, curl_socket_t socket) noexcept
{
	const auto self = static_cast<BasicSession*>(self_ptr);

	CURLIO_INFO("Closing socket #" << socket);
	if (const auto it = self->_sockets.find(socket); it != self->_sockets.end()) {
		detail::asio_error_code ec{};
		it->second->socket.close(ec);
		self->_sockets.erase(it);
		if (ec) {
			return CURLE_UNKNOWN_OPTION;
		}
	} else {
		CURLIO_WARN("Socket #" << socket << " is unkown in socket map");
		return close(socket);
	}

	return CURLE_OK;
}

} // namespace cURLio
