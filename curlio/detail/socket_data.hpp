#pragma once

#include "asio_include.hpp"

namespace curlio::detail {

struct Socket_data {
	enum Wait_flag {
		wait_flag_write = 0x1,
		wait_flag_read  = 0x2,
	};

	CURLIO_ASIO_NS::ip::tcp::socket socket;
	int wait_flags = 0;
};

} // namespace curlio::detail
