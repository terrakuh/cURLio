#pragma once

#include "config.hpp"
#include "detail/asio_include.hpp"
#include "detail/function.hpp"
#include "detail/option_type.hpp"
#include "fwd.hpp"

#include <curl/curl.h>

namespace cURLio {

template<typename Executor>
class BasicRequest {
public:
	using executor_type = Executor;
	using strand_type   = CURLIO_ASIO_NS::strand<executor_type>;

	BasicRequest(BasicSession<Executor>& session);
	BasicRequest(const BasicRequest& copy);
	BasicRequest(BasicRequest&& move) = delete;
	~BasicRequest();

	/// Sets cURL option and checks the result.
	template<CURLoption Option>
	void set_option(detail::option_type<Option> value);
	/// Appends the given header value (e.g. `"User-Agent: me"`) to cURL header list.
	void append_header(const char* header);
	/// Frees all headers.
	void free_headers() noexcept;
	/// Sends some of the given buffer (ASIO `ConstBufferSequence`) to the remote.
	auto async_write_some(const auto& buffers, auto&& token);
	auto async_abort(auto&& token);
	CURLIO_NO_DISCARD CURL* native_handle() const noexcept;
	CURLIO_NO_DISCARD executor_type get_executor() const noexcept;
	CURLIO_NO_DISCARD strand_type& get_strand() noexcept;

	BasicRequest& operator=(const BasicRequest& copy) = delete;
	BasicRequest& operator=(BasicRequest&& move)      = delete;

private:
	friend class BasicSession<Executor>;
	friend class BasicResponse<Executor>;

	std::shared_ptr<strand_type> _strand;
	// The CURL easy handle. The response owns this instance.
	CURL* _handle;
	curl_slist* _additional_headers = nullptr;
	/// An optional handler waiting to send more data.
	detail::Function<std::size_t(detail::asio_error_code, char*, std::size_t)> _send_handler{};

	BasicRequest(std::shared_ptr<BasicSession<Executor>>&& session);
	void _mark_finished() noexcept;
	static std::size_t _read_callback(char* data, std::size_t size, std::size_t count, void* self_ptr) noexcept;
};

using Request = BasicRequest<CURLIO_ASIO_NS::any_io_executor>;

} // namespace cURLio
