#pragma once

#include "detail/asio_include.hpp"
#include "error.hpp"

#include <cassert>

#if defined(CURLIO_ENABLE_LOGGING)
#	include <iostream>
#	include <thread>

// #	define CURLIO_TRACE(stream) static_cast<void>(0)
#	define CURLIO_ASSERT(stmt)                                                                                \
		std::cout << "ASSERT " << std::this_thread::get_id() << "  (" #stmt "): " << ((stmt) ? "ok" : "fail")    \
		          << "\n";                                                                                       \
		assert(stmt)
#	define CURLIO_TRACE(stream) std::cout << "TRACE  " << std::this_thread::get_id() << ": " << stream << "\n"
#	define CURLIO_DEBUG(stream) std::cout << "DEBUG  " << std::this_thread::get_id() << ": " << stream << "\n"
#	define CURLIO_INFO(stream) std::cout << "INFO   " << std::this_thread::get_id() << ": " << stream << "\n"
#	define CURLIO_WARN(stream) std::cout << "WARN   " << std::this_thread::get_id() << ": " << stream << "\n"
#	define CURLIO_ERROR(stream) std::cout << "ERROR  " << std::this_thread::get_id() << ": " << stream << "\n"
#else
#	define CURLIO_ASSERT(stmt) assert(stmt)
#	define CURLIO_TRACE(stream) static_cast<void>(0)
#	define CURLIO_DEBUG(stream) static_cast<void>(0)
#	define CURLIO_INFO(stream) static_cast<void>(0)
#	define CURLIO_WARN(stream) static_cast<void>(0)
#	define CURLIO_ERROR(stream) static_cast<void>(0)
#endif

#define CURLIO_MULTI_ASSERT(expr)                                                                            \
	if (const auto err = expr; err != CURLM_OK) {                                                              \
		CURLIO_ERROR("Function " #expr " failed: " << curl_multi_strerror(err));                                 \
		throw std::system_error{ static_cast<::curlio::Code>(                                                    \
			static_cast<int>(::curlio::Code::curl_multi_reserved) + static_cast<int>(err) + 1) };                  \
	}
#define CURLIO_MULTI_CHECK(expr)                                                                             \
	[&] {                                                                                                      \
		const auto err = expr;                                                                                   \
		if (err != CURLM_OK) {                                                                                   \
			CURLIO_ERROR("Function " #expr " failed: " << curl_multi_strerror(err));                               \
			return ::curlio::detail::asio_error_code{ static_cast<::curlio::Code>(                                 \
				static_cast<int>(::curlio::Code::curl_multi_reserved) + static_cast<int>(err) + 1) };                \
		}                                                                                                        \
		return ::curlio::detail::asio_error_code{};                                                              \
	}()

#define CURLIO_EASY_ASSERT(expr)                                                                             \
	if (const auto err = expr; err != CURLE_OK) {                                                              \
		CURLIO_ERROR("Function " #expr " failed: " << curl_easy_strerror(err));                                  \
		throw std::system_error{ static_cast<::curlio::Code>(                                                    \
			static_cast<int>(::curlio::Code::curl_easy_reserved) + static_cast<int>(err)) };                       \
	}
#define CURLIO_EASY_CHECK(expr)                                                                              \
	[&] {                                                                                                      \
		const auto err = expr;                                                                                   \
		if (err != CURLE_OK) {                                                                                   \
			CURLIO_ERROR("Function " #expr " failed: " << curl_easy_strerror(err));                                \
			return ::curlio::detail::asio_error_code{ static_cast<::curlio::Code>(                                 \
				static_cast<int>(::curlio::Code::curl_easy_reserved) + static_cast<int>(err)) };                     \
		}                                                                                                        \
		return ::curlio::detail::asio_error_code{};                                                              \
	}()
