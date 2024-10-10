#pragma once

#include "config.hpp"
#include "detail/asio_include.hpp"
#include "detail/socket_data.hpp"
#include "fwd.hpp"

#include <curl/curl.h>
#include <map>
#include <memory>

namespace cURLio {

template<typename Executor>
class BasicSession {
public:
	using executor_type    = Executor;
	using strand_type      = CURLIO_ASIO_NS::strand<executor_type>;
	using request_pointer  = std::shared_ptr<BasicRequest<Executor>>;
	using response_pointer = std::shared_ptr<BasicResponse<Executor>>;

	BasicSession(Executor executor);
	BasicSession(const BasicSession& copy) = delete;
	BasicSession(BasicSession&& move)      = delete;
	~BasicSession();

	auto async_start(request_pointer request, auto&& token);
	CURLIO_NO_DISCARD executor_type get_executor() const noexcept;
	CURLIO_NO_DISCARD strand_type& get_strand() noexcept;

	BasicSession& operator=(const BasicSession& copy) = delete;
	BasicSession& operator=(BasicSession&& move)      = delete;

private:
	friend class BasicRequest<Executor>;

	CURLM* _multi_handle;
	std::shared_ptr<strand_type> _strand;
	std::map<CURL*, response_pointer> _active_requests;
	std::map<curl_socket_t, std::shared_ptr<detail::SocketData>> _sockets;
	CURLIO_ASIO_NS::steady_timer _timer{ *_strand };

	void _monitor(const std::shared_ptr<detail::SocketData>& data, detail::SocketData::WaitFlag type) noexcept;
	void _clean_finished() noexcept;
	void _perform(curl_socket_t socket, int bitmask) noexcept;
	static int _socket_callback(CURL* easy_handle, curl_socket_t socket, int what, void* self_ptr,
	                            void* socket_data_ptr) noexcept;
	static int _timer_callback(CURLM* multi_handle, long timeout_ms, void* self_ptr) noexcept;
	static curl_socket_t _open_socket_callback(void* self_ptr, curlsocktype purpose,
	                                           curl_sockaddr* address) noexcept;
	static int _close_socket_callback(void* self_ptr, curl_socket_t socket) noexcept;
};

using Session = BasicSession<CURLIO_ASIO_NS::any_io_executor>;

} // namespace cURLio
