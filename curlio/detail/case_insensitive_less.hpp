#pragma once

#include <locale>
#include <string>

namespace curlio::detail {

struct CaseInsensitiveLess {
	std::locale locale{};

	bool operator()(const std::string& lhs, const std::string& rhs) const noexcept
	{
		if (lhs.size() < rhs.size()) {
			return true;
		} else if (lhs.size() > rhs.size()) {
			return false;
		}
		for (std::size_t i = 0; i < lhs.size(); ++i) {
			const char l = std::toupper(lhs[i], locale);
			const char r = std::toupper(rhs[i], locale);
			if (l < r) {
				return true;
			} else if (l > r) {
				return false;
			}
		}
		return false;
	}
};

} // namespace curlio::detail
