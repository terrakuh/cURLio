#pragma once

#include "config.hpp"
#include "detail/asio_include.hpp"
#include "detail/function.hpp"
#include "detail/header_collector.hpp"
#include "fwd.hpp"

#include <curl/curl.h>
#include <memory>

namespace cURLio {

using Headers = detail::HeaderCollector::fields_type;

template<typename Executor>
class BasicResponse : public std::enable_shared_from_this<BasicResponse<Executor>> {
public:
	using executor_type = Executor;
	using strand_type   = CURLIO_ASIO_NS::strand<executor_type>;

	BasicResponse(const BasicResponse& copy) = delete;
	BasicResponse(BasicResponse&& move)      = delete;

	/// Returns information about from the easy handle. Access is synchronized.
	template<CURLINFO Option>
	auto async_get_info(auto&& token) const;
	/// Reads some data from the remote and stores it in the given buffer (ASIO `MutableBufferSequence`).
	auto async_read_some(const auto& buffers, auto&& token);
	/// Waits until a complete header section is received. This could be the first or the last if this is a
	/// redirect depending on the settings.
	auto async_wait_headers(auto&& token);
	CURLIO_NO_DISCARD executor_type get_executor() const noexcept;
	CURLIO_NO_DISCARD strand_type& get_strand() noexcept;

	BasicResponse& operator=(const BasicResponse& copy) = delete;
	BasicResponse& operator=(BasicResponse&& move)      = delete;

private:
	// Only the session may construct a response and call `_start()` / `_stop()`.
	friend class BasicSession<Executor>;

	std::shared_ptr<strand_type> _strand;
	std::shared_ptr<BasicRequest<Executor>> _request;
	CURLIO_ASIO_NS::streambuf _input_buffer{};
	detail::Function<std::size_t(detail::asio_error_code, const char*, std::size_t)> _receive_handler{};
	detail::HeaderCollector _header_collector;
	bool _finished = false;

	BasicResponse(std::shared_ptr<strand_type> strand,
	              std::shared_ptr<BasicRequest<Executor>> request) noexcept;
	[[nodiscard]] detail::asio_error_code _start() noexcept;
	[[nodiscard]] detail::asio_error_code _stop() noexcept;
	static std::size_t _write_callback(char* data, std::size_t size, std::size_t count,
	                                   void* self_ptr) noexcept;
};

template<typename Executor>
auto async_wait_last_headers(std::shared_ptr<BasicResponse<Executor>> response, auto&& token);

using Response = BasicResponse<CURLIO_ASIO_NS::any_io_executor>;

} // namespace cURLio
