#pragma once

#include "config.hpp"
#include "detail/asio_include.hpp"
#include "detail/function.hpp"
#include "fwd.hpp"
#include "detail/option_type.hpp"

#include <curl/curl.h>

namespace curlio {

template<typename Executor>
class Basic_request : public std::enable_shared_from_this<Basic_request<Executor>> {
public:
	using executor_type = Executor;

	Basic_request(const Basic_request& copy);
	~Basic_request() noexcept;

	template<CURLoption Option>
	void set_option(detail::option_type<Option> value);
	void append_header(const char* header);
	auto async_write_some(const auto& buffers, auto&& token);
	auto async_abort(auto&& token);
	CURLIO_NO_DISCARD CURL* native_handle() const noexcept;
	CURLIO_NO_DISCARD executor_type get_executor() const noexcept;
	Basic_request& operator=(const Basic_request& copy) = delete;

	template<typename Executor_>
	friend std::shared_ptr<Basic_request<Executor_>>
	  make_request(std::shared_ptr<Basic_session<Executor_>> session);

private:
	friend class Basic_session<Executor>;

	std::shared_ptr<Basic_session<Executor>> _session;
	CURL* _handle;
	curl_slist* _additional_headers = nullptr;
	detail::Function<std::size_t(detail::asio_error_code, char*, std::size_t)> _send_handler;

	Basic_request(std::shared_ptr<Basic_session<Executor>>&& session);
	void _mark_finished();
	static std::size_t _read_callback(char* data, std::size_t size, std::size_t count, void* self_ptr) noexcept;
};

using Request = Basic_request<CURLIO_ASIO_NS::any_io_executor>;

} // namespace curlio
