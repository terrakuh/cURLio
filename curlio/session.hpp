#pragma once

#include "detail/curl_share_lock.hpp"
#include "error.hpp"
#include "log.hpp"
#include "request.hpp"

#include <boost/asio.hpp>
#include <curl/curl.h>
#include <map>
#include <string>

namespace curlio {

class Session
{
public:
	typedef boost::asio::any_io_executor executor_type;

	Session(boost::asio::any_io_executor executor);
	Session(Session&& move) = delete;
	~Session() noexcept;

	bool is_valid() const noexcept { return _multi_handle != nullptr; }
	/// Starts the request. Make sure all data is read and the request is awaited.
	void start(Request& request);
	void set_cookie_file(std::string file) { _cookie_file = file; }
	boost::asio::any_io_executor get_executor() noexcept { return _timer.get_executor(); }
	Session& operator=(Session&& move) = delete;

private:
	boost::asio::steady_timer _timer;
	CURLM* _multi_handle  = nullptr;
	CURLSH* _share_handle = nullptr;
	detail::CURL_share_lock _share_lock;
	/// Path to the cookie jar or empty.
	std::string _cookie_file;
	/// All active connections.
	std::map<curl_socket_t, boost::asio::ip::tcp::socket> _sockets;

	void _async_wait(boost::asio::ip::tcp::socket& socket, boost::asio::socket_base::wait_type type);
	/// Call Request::_finish() for every finished request and remove it from the multi-handle.
	void _clean_finished();
	void _handle_timeout(boost::system::error_code ec);
	static int _socket_callback(CURL* handle, curl_socket_t socket, int what, void* self_pointer,
	                            void* socket_pointer);
	static int _multi_timer_callback(CURLM* multi, long timeout_ms, void* self_pointer);
	/// Called by cURL to open a new connection.
	static curl_socket_t _open_socket(void* self_pointer, curlsocktype purpose,
	                                  struct curl_sockaddr* address) noexcept;
	static int _close_socket(void* self_pointer, curl_socket_t socket) noexcept;
};

inline Session::Session(boost::asio::any_io_executor executor) : _timer{ executor }
{
	_multi_handle = curl_multi_init();
	curl_multi_setopt(_multi_handle, CURLMOPT_SOCKETFUNCTION, &_socket_callback);
	curl_multi_setopt(_multi_handle, CURLMOPT_SOCKETDATA, this);
	curl_multi_setopt(_multi_handle, CURLMOPT_TIMERFUNCTION, &_multi_timer_callback);
	curl_multi_setopt(_multi_handle, CURLMOPT_TIMERDATA, this);

	_share_handle = curl_share_init();
	curl_share_setopt(_share_handle, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE);
	curl_share_setopt(_share_handle, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
	curl_share_setopt(_share_handle, CURLSHOPT_LOCKFUNC, &detail::CURL_share_lock::lock);
	curl_share_setopt(_share_handle, CURLSHOPT_UNLOCKFUNC, &detail::CURL_share_lock::unlock);
	curl_share_setopt(_share_handle, CURLSHOPT_USERDATA, &_share_lock);
}

inline Session::~Session() noexcept
{
	if (_multi_handle != nullptr) {
		curl_multi_cleanup(_multi_handle);
	}
	if (_share_handle != nullptr) {
		curl_share_cleanup(_share_handle);
	}
}

inline void Session::start(Request& request)
{
	if (request._executor) {
		throw std::system_error{ Code::request_in_use };
	}

	CURLIO_DEBUG("Starting request " << &request);
	const auto easy_handle = request.native_handle();
	request._executor      = get_executor();
	curl_easy_setopt(easy_handle, CURLOPT_OPENSOCKETFUNCTION, &Session::_open_socket);
	curl_easy_setopt(easy_handle, CURLOPT_OPENSOCKETDATA, this);
	curl_easy_setopt(easy_handle, CURLOPT_CLOSESOCKETFUNCTION, &Session::_close_socket);
	curl_easy_setopt(easy_handle, CURLOPT_CLOSESOCKETDATA, this);
	curl_easy_setopt(easy_handle, CURLOPT_COOKIEFILE, _cookie_file.c_str());
	if (!_cookie_file.empty()) {
		CURLIO_DEBUG("Setting cookie file '" << _cookie_file << "' for request " << &request);
		curl_easy_setopt(easy_handle, CURLOPT_COOKIEJAR, _cookie_file.c_str());
	}
	curl_easy_setopt(easy_handle, CURLOPT_SHARE, _share_handle);

	curl_multi_add_handle(_multi_handle, easy_handle);

	int running = 0;
	curl_multi_socket_action(_multi_handle, CURL_SOCKET_TIMEOUT, 0, &running);
	CURLIO_DEBUG("Kick-off timeout. Running=" << running);
}

inline void Session::_async_wait(boost::asio::ip::tcp::socket& socket,
                                 boost::asio::socket_base::wait_type type)
{
	socket.async_wait(type, [this, type, &socket](boost::system::error_code ec) {
		if (!ec) {
			const auto handle = socket.native_handle();
			int still_running = 0;
			curl_multi_socket_action(_multi_handle, handle,
			                         type == boost::asio::socket_base::wait_read ? CURL_POLL_IN : CURL_POLL_OUT,
			                         &still_running);
			_clean_finished();
			if (still_running <= 0) {
				_timer.cancel();
			}

			if (_sockets.find(handle) != _sockets.end()) {
				_async_wait(socket, type);
			}
		}
	});
}

inline void Session::_clean_finished()
{
	CURLIO_TRACE("Cleaning finished requests");
	CURLMsg* message = nullptr;
	int left         = 0;
	while ((message = curl_multi_info_read(_multi_handle, &left))) {
		if (message->msg == CURLMSG_DONE) {
			Request* request = nullptr;
			curl_easy_getinfo(message->easy_handle, CURLINFO_PRIVATE, &request);
			CURLIO_DEBUG("Request " << request << " is done");
			if (request != nullptr) {
				request->_finish();
			}
			curl_multi_remove_handle(_multi_handle, message->easy_handle);
		}
	}
}

inline void Session::_handle_timeout(boost::system::error_code ec)
{
	CURLIO_TRACE("Socket timeouted with ec=" << ec);
	if (!ec) {
		int still_running = 0;
		curl_multi_socket_action(_multi_handle, CURL_SOCKET_TIMEOUT, 0, &still_running);
		CURLIO_DEBUG(still_running << " requests still running");
		_clean_finished();
	}
}

inline int Session::_socket_callback(CURL* handle, curl_socket_t socket, int what, void* self_pointer,
                                     void* socket_pointer)
{
	const auto self = static_cast<Session*>(self_pointer);
	const auto it   = self->_sockets.find(socket);
	if (it == self->_sockets.end()) {
		return 0;
	}

	switch (what) {
	case CURL_POLL_IN: self->_async_wait(it->second, boost::asio::socket_base::wait_read); break;
	case CURL_POLL_OUT: self->_async_wait(it->second, boost::asio::socket_base::wait_write); break;
	case CURL_POLL_INOUT: {
		self->_async_wait(it->second, boost::asio::socket_base::wait_read);
		self->_async_wait(it->second, boost::asio::socket_base::wait_write);
		break;
	}
	case CURL_POLL_REMOVE: it->second.cancel(); break;
	}
	return 0;
}

inline int Session::_multi_timer_callback(CURLM* multi, long timeout_ms, void* self_pointer)
{
	CURLIO_DEBUG("Starting timeout with " << timeout_ms << " ms");
	const auto self = static_cast<Session*>(self_pointer);

	self->_timer.expires_from_now(std::chrono::milliseconds{ timeout_ms });
	if (timeout_ms > 0) {
		self->_timer.async_wait([self](boost::system::error_code ec) { return self->_handle_timeout(ec); });
	} else {
		self->_handle_timeout({});
	}
	return 0;
}

inline curl_socket_t Session::_open_socket(void* self_pointer, curlsocktype purpose,
                                           struct curl_sockaddr* address) noexcept
{
	CURLIO_TRACE("Trying to open socket");
	const auto self = static_cast<Session*>(self_pointer);
	if (purpose == CURLSOCKTYPE_IPCXN) {
		if (address->family == AF_INET) {
			boost::system::error_code ec;
			boost::asio::ip::tcp::socket socket{ self->get_executor() };
			socket.open(boost::asio::ip::tcp::v4(), ec);
			if (!ec) {
				auto fd = socket.native_handle();
				CURLIO_INFO("New socket opened: " << fd);
				self->_sockets.insert(std::make_pair(fd, std::move(socket)));
				return fd;
			}
		}
	}
	CURLIO_ERROR("Failed to open socket. Purpose=" << purpose << ", family=" << address->family);
	return CURL_SOCKET_BAD;
}

inline int Session::_close_socket(void* self_pointer, curl_socket_t socket) noexcept
{
	CURLIO_INFO("Closing socket: " << socket);
	const auto self = static_cast<Session*>(self_pointer);
	self->_sockets.erase(socket);
	return 0;
}

} // namespace curlio
