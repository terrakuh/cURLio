#pragma once

#include "config.hpp"
#include "detail/asio_include.hpp"
#include "detail/function.hpp"
#include "detail/header_collector.hpp"
#include "detail/option_type.hpp"
#include "fwd.hpp"

#include <curl/curl.h>
#include <map>
#include <memory>

namespace curlio {

template<typename Executor>
class Basic_response : public std::enable_shared_from_this<Basic_response<Executor>> {
public:
	using executor_type = Executor;
	using headers_type  = detail::Header_collector::fields_type;

	Basic_response(const Basic_response& copy) = delete;

	template<CURLINFO Option>
	auto get_info() const;
	template<CURLINFO Option>
	auto async_get_info(auto&& token) const;
	auto async_read_some(const auto& buffers, auto&& token);
	auto async_wait_headers(auto&& token);
	CURLIO_NO_DISCARD executor_type get_executor() const noexcept;
	Basic_response& operator=(const Basic_response& copy) = delete;

private:
	friend class Basic_session<Executor>;

	std::shared_ptr<Basic_session<Executor>> _session;
	std::shared_ptr<Basic_request<Executor>> _request;
	CURLIO_ASIO_NS::streambuf _input_buffer;
	detail::Function<std::size_t(detail::asio_error_code, const char*, std::size_t)> _receive_handler;
	detail::Header_collector _header_collector;
	bool _finished = false;

	Basic_response(std::shared_ptr<Basic_session<Executor>>&& session,
	               std::shared_ptr<Basic_request<Executor>>&& request);
	void _mark_finished();
	static std::size_t _write_callback(char* data, std::size_t size, std::size_t count,
	                                   void* self_ptr) noexcept;
};

template<typename Executor>
auto async_wait_last_headers(std::shared_ptr<Basic_response<Executor>> response, auto&& token);

using Response = Basic_response<CURLIO_ASIO_NS::any_io_executor>;

} // namespace curlio
