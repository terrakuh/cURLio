#pragma once

#include <type_traits>

namespace curlio::detail {

template<int Test, int... Values>
struct Contains : std::false_type {};

template<int Test, int... Rest>
struct Contains<Test, Test, Rest...> : std::true_type {};

template<int Test, int Other, int... Rest>
struct Contains<Test, Other, Rest...> : Contains<Test, Rest...> {};

template<int Test, int... Values>
constexpr bool contains = Contains<Test, Values...>::value;

} // namespace curlio::detail
