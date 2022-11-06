#pragma once

#include <type_traits>
#include <utility>

namespace curlio::detail {

template<typename Action>
class [[nodiscard]] Final_action {
public:
	constexpr Final_action(auto&& action) : _run{ true }, _action{ std::forward<decltype(action)>(action) } {}
	constexpr Final_action(Final_action&& move) : _action{ std::move(move._action) }
	{
		std::swap(_run, move._run);
	}
	~Final_action() noexcept
	{
		if (_run) {
			_action();
		}
	}

	constexpr void cancel() noexcept { _run = false; }
	constexpr Final_action& operator=(Final_action&& move)
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
	return Final_action<std::decay_t<decltype(action)>>{ std::forward<decltype(action)>(action) };
}

} // namespace curlio::detail
