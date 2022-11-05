#pragma once

#include <boost/asio.hpp>

namespace curlio::detail {

struct Socket_data {
	enum Wait_flag {
		wait_flag_write = 0x1,
		wait_flag_read  = 0x2,
	};

	boost::asio::ip::tcp::socket socket;
	int wait_flags = 0;
};

} // namespace curlio::detail
