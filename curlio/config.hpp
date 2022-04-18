#pragma once

#if __cplus_plus > 201703L
#	define CURLIO_NO_DISCARD [[nodiscard]]
#else
#	define CURLIO_NO_DISCARD
#endif
