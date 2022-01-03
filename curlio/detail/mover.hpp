#pragma once

#include <utility>

namespace curlio::detail {

template<typename Type>
class Mover
{
public:
	Mover() = default;
	Mover(Mover&& move) : _value{ std::move(move._value) } { move._value = Type{}; }

	Type& get() noexcept { return _value; }
	const Type& get() const noexcept { return _value; }
	operator Type&() noexcept { return get(); }
	operator const Type&() const noexcept { return get(); }
	Mover& operator=(const Type& value)
	{
		_value = value;
		return *this;
	}
	Mover& operator=(Type&& value)
	{
		_value = std::exchange(value, Type{});
		return *this;
	}
	Mover& operator=(Mover&& move)
	{
		_value = std::exchange(move._value, Type{});
		return *this;
	}

private:
	Type _value{};
};

} // namespace curlio::detail
