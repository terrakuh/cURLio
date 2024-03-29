#pragma once

#include "basic_request.hpp"
#include "basic_response.hpp"
#include "basic_session.hpp"
#include "log.hpp"

#include <functional>
#include <optional>

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
	return CURLIO_ASIO_NS::async_initiate<decltype(token),
	                                      void(detail::asio_error_code, std::shared_ptr<Response>)>(
	  [this, request = std::move(request)](auto handler) mutable {
		  CURLIO_ASIO_NS::dispatch(
		    _strand, [this, request = std::move(request), handler = std::move(handler)]() mutable {
			    const auto easy_handle = request->native_handle();

			    // TODO error
			    request->template set_option<CURLOPT_OPENSOCKETFUNCTION>(&Basic_session::_open_socket_callback);
			    request->template set_option<CURLOPT_OPENSOCKETDATA>(this);
			    request->template set_option<CURLOPT_CLOSESOCKETFUNCTION>(&Basic_session::_close_socket_callback);
			    request->template set_option<CURLOPT_CLOSESOCKETDATA>(this);

			    // Kick start.
			    CURLIO_TRACE("Starting handle " << easy_handle);
			    curl_multi_add_handle(_multi_handle, easy_handle);
			    _perform(CURL_SOCKET_TIMEOUT, 0);

			    std::shared_ptr<Response> response{ new Response{ this->shared_from_this(), std::move(request) } };
			    _active_requests.insert({ easy_handle, response });

			    auto executor = CURLIO_ASIO_NS::get_associated_executor(handler, get_executor());
			    CURLIO_ASIO_NS::post(std::move(executor),
			                         std::bind(std::move(handler), detail::asio_error_code{}, std::move(response)));
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
inline CURLIO_ASIO_NS::strand<Executor>& Basic_session<Executor>::get_strand() noexcept
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
inline void Basic_session<Executor>::_monitor(const std::shared_ptr<detail::Socket_data>& data,
                                              detail::Socket_data::Wait_flag type)
{
	if (data->wait_flags & type) {
		data->socket.async_wait(
		  type == detail::Socket_data::wait_flag_write ? CURLIO_ASIO_NS::socket_base::wait_write
		                                               : CURLIO_ASIO_NS::socket_base::wait_read,
		  [this, type, data](const detail::asio_error_code& ec) {
			  CURLIO_TRACE("Socket action occurred: " << ec.what());
			  if (!ec && data->wait_flags & type) {
				  _perform(data->socket.native_handle(),
				           type == detail::Socket_data::wait_flag_write ? CURL_CSELECT_OUT : CURL_CSELECT_IN);
				  _monitor(data, type);
			  }
		  });
	}
}

template<typename Executor>
inline void Basic_session<Executor>::_clean_finished()
{
	CURLMsg* message = nullptr;
	int left         = 0;
	while ((message = curl_multi_info_read(_multi_handle, &left))) {
		if (message->msg == CURLMSG_DONE) {
			CURLIO_INFO("Removing handle: " << message->easy_handle);

			const auto it = _active_requests.find(message->easy_handle);
			if (it != _active_requests.end()) {
				it->second->_mark_finished();
				_active_requests.erase(it);

				curl_easy_setopt(message->easy_handle, CURLOPT_OPENSOCKETFUNCTION, nullptr);
				curl_easy_setopt(message->easy_handle, CURLOPT_OPENSOCKETDATA, nullptr);
				curl_easy_setopt(message->easy_handle, CURLOPT_CLOSESOCKETFUNCTION, nullptr);
				curl_easy_setopt(message->easy_handle, CURLOPT_CLOSESOCKETDATA, nullptr);
			}

			curl_multi_remove_handle(_multi_handle, message->easy_handle);
		}
	}
}

template<typename Executor>
inline void Basic_session<Executor>::_perform(curl_socket_t socket, int bitmask)
{
	int running = 0;
	if (const auto code = curl_multi_socket_action(_multi_handle, socket, bitmask, &running);
	    code != CURLM_OK) {
		CURLIO_ERROR("Action failed: " << curl_multi_strerror(code));
	}
	CURLIO_TRACE("Action performed for " << socket << " with bitmask " << bitmask << " and remaining handles "
	                                     << running);
	_clean_finished();
}

template<typename Executor>
inline int Basic_session<Executor>::_socket_callback(CURL* easy_handle, curl_socket_t socket, int what,
                                                     void* self_ptr, void* socket_data_ptr) noexcept
{
	constexpr const char* what_names[] = { "IN", "OUT", "IN/OUT", "REMOVE" };
	CURLIO_TRACE("Socket action callback on " << socket << ": "
	                                          << (what >= 1 && what <= 4 ? what_names[what - 1] : "unknwon"));

	const auto self = static_cast<Basic_session*>(self_ptr);

	const auto it = self->_sockets.find(socket);
	if (it == self->_sockets.end()) {
		CURLIO_WARN("Socket " << socket << " not found");
		return CURLM_OK;
	}

	const auto& data = it->second;
	data->wait_flags = 0;

	if (what == CURL_POLL_REMOVE) {
		data->socket.cancel();
		return CURLM_OK;
	}

	if (what == CURL_POLL_IN || what == CURL_POLL_INOUT) {
		data->wait_flags |= detail::Socket_data::wait_flag_read;
		self->_monitor(data, detail::Socket_data::wait_flag_read);
	}
	if (what == CURL_POLL_OUT || what == CURL_POLL_INOUT) {
		data->wait_flags |= detail::Socket_data::wait_flag_write;
		self->_monitor(data, detail::Socket_data::wait_flag_write);
	}

	return CURLM_OK;
}

template<typename Executor>
inline int Basic_session<Executor>::_timer_callback(CURLM* multi_handle, long timeout_ms,
                                                    void* self_ptr) noexcept
{
	CURLIO_DEBUG("Adding timer with timeout of " << timeout_ms << " ms");

	const auto self = static_cast<Basic_session*>(self_ptr)->shared_from_this();
	if (timeout_ms == -1) {
		self->_timer.cancel();
	} else {
		self->_timer.expires_after(std::chrono::milliseconds{ timeout_ms });
		self->_timer.async_wait([self](const detail::asio_error_code& ec) {
			CURLIO_DEBUG("Timeout with ec=" << ec.message());
			if (!ec) {
				self->_perform(CURL_SOCKET_TIMEOUT, 0);
			}
		});
	}

	return 0;
}

template<typename Executor>
inline curl_socket_t Basic_session<Executor>::_open_socket_callback(void* self_ptr, curlsocktype purpose,
                                                                    curl_sockaddr* address) noexcept
{
	CURLIO_TRACE("Trying to open new socket with family=" << address->family);
	const auto self = static_cast<Basic_session*>(self_ptr);

	std::optional<CURLIO_ASIO_NS::ip::tcp> protocol{};
	if (address->family == AF_INET) {
		protocol = CURLIO_ASIO_NS::ip::tcp::v4();
	} else if (address->family == AF_INET6) {
		protocol = CURLIO_ASIO_NS::ip::tcp::v6();
	}

	if (protocol.has_value()) {
		detail::asio_error_code ec{};
		auto data = std::make_shared<detail::Socket_data>(CURLIO_ASIO_NS::ip::tcp::socket{ self->_strand });
		data->socket.open(protocol.value(), ec);
		if (!ec) {
			const auto fd = data->socket.native_handle();
			CURLIO_INFO("New socket opened: " << fd);
			self->_sockets.insert({ fd, std::move(data) });
			return fd;
		}
	}

	CURLIO_ERROR("Failed to open socket. Purpose=" << purpose << ", family=" << address->family);
	return CURL_SOCKET_BAD;
}

template<typename Executor>
inline int Basic_session<Executor>::_close_socket_callback(void* self_ptr, curl_socket_t socket) noexcept
{
	const auto self = static_cast<Basic_session*>(self_ptr);

	CURLIO_INFO("Closing socket: " << socket);
	if (const auto it = self->_sockets.find(socket); it != self->_sockets.end()) {
		detail::asio_error_code ec{};
		it->second->socket.close(ec);
		self->_sockets.erase(it);
		if (ec) {
			return CURLE_UNKNOWN_OPTION;
		}
	} else {
		return close(socket);
	}

	return CURLE_OK;
}

template<typename Executor>
CURLIO_NO_DISCARD inline std::shared_ptr<Basic_session<Executor>> make_session(Executor executor)
{
	return std::shared_ptr<Basic_session<Executor>>{ new Basic_session<Executor>{ std::move(executor) } };
}

} // namespace curlio
