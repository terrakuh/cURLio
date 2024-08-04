#pragma once

#include <type_traits>
#include <utility>

namespace curlio::detail {

template<typename Action>
class [[nodiscard]] FinalAction {
public:
	constexpr FinalAction(auto&& action) : _run{ true }, _action{ std::forward<decltype(action)>(action) } {}
	constexpr FinalAction(FinalAction&& move) : _action{ std::move(move._action) }
	{
		std::swap(_run, move._run);
	}
	~FinalAction() noexcept
	{
		if (_run) {
			_action();
		}
	}

	constexpr void cancel() noexcept { _run = false; }
	constexpr FinalAction& operator=(FinalAction&& move)
	{
		_action = std::move(move._action);
		_run    = std::exchange(move._run, false);
		return *this;
	}

private:
	bool _run = false;
	Action _action;
};

[[nodiscard]] constexpr auto finally(auto&& action)
{
	return FinalAction<std::decay_t<decltype(action)>>{ std::forward<decltype(action)>(action) };
}

} // namespace curlio::detail
