/**
 * @file
 *
 * Error codes and conditions.
 */
#pragma once

#include <curl/curl.h>
#include <system_error>
#include <type_traits>

namespace curlio {

enum class Code {
	success,

	multiple_reads,
	multiple_writes,
	multiple_headers_awaitings,
	multiple_completion_awaitings,
	request_in_use,
	request_not_active,
	bad_url,
	no_response_code,

	/// From 1000 - 2000 reserved for CURL easy errors.
	curl_easy_reserved = 1000,
	/// From 2000 - 3000 reserved for CURL easy errors.
	curl_multi_reserved = 2000,
};

enum class Condition {
	success,
	usage,
	curl_easy,
	curl_multi,
};

std::error_condition make_error_condition(Condition condition) noexcept;

inline const std::error_category& code_category() noexcept
{
	static class : public std::error_category {
	public:
		const char* name() const noexcept override { return "curlio"; }
		std::error_condition default_error_condition(int code) const noexcept override
		{
			if (code == 0) {
				return make_error_condition(Condition::success);
			} else if (code >= 1 && code < 50) {
				return make_error_condition(Condition::usage);
			} else if (code >= 1000 && code < 2000) {
				return make_error_condition(Condition::curl_easy);
			} else if (code >= 2000 && code < 3000) {
				return make_error_condition(Condition::curl_multi);
			}
			return error_category::default_error_condition(code);
		}
		std::string message(int ec) const override
		{
			// Special handling for CURL errors.
			if (ec >= 1000 && ec < 2000) {
				return curl_easy_strerror(static_cast<CURLcode>(ec - 1000));
			} else if (ec >= 2000 && ec < 3000) {
				return curl_multi_strerror(static_cast<CURLMcode>(ec - 2000 - 1));
			}

			switch (static_cast<Code>(ec)) {
			case Code::success: return "success";

			case Code::multiple_reads: return "multiple read operations not allowed";
			case Code::multiple_writes: return "multiple write operations not allowed";
			case Code::multiple_headers_awaitings: return "multiple header await operations not allowed";
			case Code::multiple_completion_awaitings: return "multiple completion await operations not allowed";
			case Code::request_in_use: return "request is already in use";
			case Code::request_not_active: return "request is not active";
			case Code::bad_url: return "bad URL";
			case Code::no_response_code: return "no response code available";

			default: return "(unrecognized error code)";
			}
		}
	} category;
	return category;
}

inline const std::error_category& condition_category() noexcept
{
	static class : public std::error_category {
	public:
		const char* name() const noexcept override { return "curlio"; }
		std::string message(int condition) const override
		{
			switch (static_cast<Condition>(condition)) {
			case Condition::success: return "success";
			case Condition::usage: return "usage";
			case Condition::curl_easy: return "CURL easy";
			default: return "(unrecognized error condition)";
			}
		}
	} category;
	return category;
}

inline std::error_code make_error_code(Code code) noexcept
{
	return { static_cast<int>(code), code_category() };
}

inline std::error_condition make_error_condition(Condition condition) noexcept
{
	return { static_cast<int>(condition), condition_category() };
}

} // namespace curlio

namespace std {

template<>
struct is_error_code_enum<curlio::Code> : std::true_type {};

template<>
struct is_error_condition_enum<curlio::Condition> : std::true_type {};

} // namespace std
