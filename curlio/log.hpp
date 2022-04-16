#pragma once

#if defined(CURLIO_ENABLE_LOGGING)
#	include <iostream>

#	define CURLIO_TRACE(stream) std::cout << "TRACE: " << stream << "\n"
#	define CURLIO_DEBUG(stream) std::cout << "DEBUG: " << stream << "\n"
#	define CURLIO_INFO(stream) std::cout << "INFO : " << stream << "\n"
#	define CURLIO_ERROR(stream) std::cout << "ERROR: " << stream << "\n"
#else
#	define CURLIO_TRACE(stream) static_cast<void>(0)
#	define CURLIO_DEBUG(stream) static_cast<void>(0)
#	define CURLIO_INFO(stream) static_cast<void>(0)
#	define CURLIO_ERROR(stream) static_cast<void>(0)
#endif
