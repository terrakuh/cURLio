#pragma once

#include <memory>
#include <type_traits>
#include <utility>

namespace cURLio::detail {

template<typename Type>
class Function;

template<typename Return, typename... Arguments>
class Invoker {
public:
	virtual ~Invoker()                            = default;
	virtual Return invoke(Arguments... arguments) = 0;
};

template<typename Functor, typename Return, typename... Arguments>
class Functor_invoker : public Invoker<Return, Arguments...> {
public:
	Functor_invoker(const Functor& functor) : _functor{ functor } {}
	Functor_invoker(Functor&& functor) : _functor{ std::move(functor) } {}
	Return invoke(Arguments... arguments) override { return _functor(std::forward<Arguments>(arguments)...); }

private:
	Functor _functor;
};

template<typename Return, typename... Arguments>
class Function<Return(Arguments...)> {
public:
	Function()                     = default;
	Function(const Function& copy) = delete;
	Function(Function&& move) : _invoker{ std::move(move._invoker) } {}
	template<typename Functor>
	Function(Functor&& functor)
	    : _invoker{ std::make_unique<Functor_invoker<typename std::decay<Functor>::type, Return, Arguments...>>(
		      std::forward<Functor>(functor)) }
	{}
	void reset() { _invoker = nullptr; }
	Return operator()(Arguments... arguments)
	{
		return _invoker->invoke(std::forward<Arguments>(arguments)...);
	}
	operator bool() const noexcept { return _invoker != nullptr; }
	Function& operator=(const Function& copy) = delete;
	Function& operator                        =(Function&& move)
	{
		_invoker = std::move(move._invoker);
		return *this;
	}
	template<typename Functor>
	Function& operator=(Functor&& functor)
	{
		_invoker = std::make_unique<Functor_invoker<typename std::decay<Functor>::type, Return, Arguments...>>(
		  std::forward<Functor>(functor));
		return *this;
	}

private:
	std::unique_ptr<Invoker<Return, Arguments...>> _invoker;
};

} // namespace cURLio::detail
