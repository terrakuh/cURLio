#pragma once

#include "config.hpp"
#include "detail/socket_data.hpp"
#include "fwd.hpp"

#include <boost/asio.hpp>
#include <curl/curl.h>
#include <map>
#include <memory>

namespace curlio {

template<typename Executor>
class Basic_session : public std::enable_shared_from_this<Basic_session<Executor>> {
public:
	using executor_type    = Executor;
	using request_pointer  = std::shared_ptr<Basic_request<Executor>>;
	using response_pointer = std::shared_ptr<Basic_response<Executor>>;

	Basic_session(const Basic_session& copy) = delete;
	~Basic_session() noexcept;

	auto async_start(request_pointer request, auto&& token);
	CURLIO_NO_DISCARD executor_type get_executor() const noexcept;
	CURLIO_NO_DISCARD boost::asio::strand<Executor>& get_strand() noexcept;
	Basic_session& operator=(const Basic_session& copy) = delete;

	template<typename Executor_>
	friend std::shared_ptr<Basic_session<Executor_>> make_session(Executor_ executor);

private:
	CURLM* _multi_handle;
	boost::asio::strand<Executor> _strand;
	std::map<CURL*, response_pointer> _active_requests;
	std::map<curl_socket_t, std::shared_ptr<detail::Socket_data>> _sockets;
	boost::asio::steady_timer _timer{ _strand };

	Basic_session(Executor executor);
	void _monitor(const std::shared_ptr<detail::Socket_data>& data, detail::Socket_data::Wait_flag type);
	void _clean_finished();
	void _perform(curl_socket_t socket, int bitmask);
	static int _socket_callback(CURL* easy_handle, curl_socket_t socket, int what, void* self_ptr,
	                            void* socket_data_ptr) noexcept;
	static int _timer_callback(CURLM* multi_handle, long timeout_ms, void* self_ptr) noexcept;
	static curl_socket_t _open_socket_callback(void* self_ptr, curlsocktype purpose,
	                                           curl_sockaddr* address) noexcept;
	static int _close_socket_callback(void* self_ptr, curl_socket_t socket) noexcept;
};

using Session = Basic_session<boost::asio::any_io_executor>;

} // namespace curlio
