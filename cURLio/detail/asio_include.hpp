#pragma once

#if defined(CURLIO_USE_STANDALONE_ASIO)
#	include <asio.hpp>
#	define CURLIO_ASIO_NS asio
#	define CURLIO_ASIO_HAS_CANCEL __has_include(<asio/cancellation_signal.hpp>)
#else // Fall back to Boost.ASIO
#	include <boost/asio.hpp>
#	define CURLIO_ASIO_NS boost::asio
#	define CURLIO_ASIO_HAS_CANCEL __has_include(<boost/asio/cancellation_signal.hpp>)
#endif

namespace curlio::detail {

using asio_error_code = decltype(make_error_code(CURLIO_ASIO_NS::error::eof));

} // namespace curlio::detail
