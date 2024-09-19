#pragma once

#include <cassert>

#if defined(CURLIO_ENABLE_LOGGING)
#	include <iostream>
#	include <thread>

// #	define CURLIO_TRACE(stream) static_cast<void>(0)
#	define CURLIO_ASSERT(stmt)                                                                                \
		std::cout << "ASSERT " << std::this_thread::get_id() << "  (" #stmt "): " << (stmt) << "\n";             \
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
