#pragma once

#include <boost/asio.hpp>

namespace curlio::detail {

struct Socket_data {
	boost::asio::ip::tcp::socket socket;
	boost::asio::cancellation_signal cancel_wait_read;
	boost::asio::cancellation_signal cancel_wait_write;
	void* user_data = nullptr;

	~Socket_data() noexcept
	{
		socket.release();
	}
};

} // namespace curlio::detail
