# cURLio

The simple C++ 17 glue between [ASIO](https://think-async.com/Asio/) and [cURL](https://curl.se/). The library is fully templated and header-only. It follows the basic principles of the ASIO design and defines `async_` functions that can take different completion handlers.

On the cURL side the [multi_socket](https://everything.curl.dev/libcurl/drive/multi-socket) approach is implemented with the `cURLio::Session` class which can handle thousands of requests in parallel. The current implementation of a session is synchronized via ASIO's strand mechanism in order to avoid concurrent access to the cURL handles in the background.

This library is only brings cURLio into the world of ASIO and is **not** meant to be easy-to-use wrapper for cURL. There are other libraries for that.

## Example

The following examples uses the [coroutines](https://en.cppreference.com/w/cpp/language/coroutines) which were introduced in C++ 20:

```cpp
#include <cURLio.hpp>

asio::io_service service{};

cURLio::Session session{ service.get_executor() };

// Create request and set options.
auto request = std::make_shared<cURLio::Request>(session);
request->set_option<CURLOPT_URL>("http://example.com");
request->set_option<CURLOPT_USERAGENT>("cURLio");

// Launches the request which will then run in the background.
auto response = co_await session.async_start(request, asio::use_awaitable);

// Read all and do something with the data.
char data[4096];
while (true) {
	const auto [ec, bytes_transferred] = co_await response->async_read_some(
	                                        asio::buffer(data), 
	                                        asio::as_tuple(asio::use_awaitable));
	if (ec) {
		break;
	}
	/* Do something. */
}

// When all data was read and the `response->async_read_some()` returned `asio::error::eof`
// the request is removed from the session and can be used again.
```

## Configuration

The library provides two CMake targets `cURLio::cURLio-asio` and `cURLio::cURLio-boost-asio` that link to the standalone ASIO and the Boost.ASIO library respectively. The target `cURLio::cURLio` is an alias depending on the value of `CURLIO_USE_STANDALONE_ASIO` (default `OFF`).

## Installation

```sh
git clone https://github.com/terrakuh/cURLio.git
cmake -S cURLio -B cURLio/build
cmake --install cURLio/build
```

And then in your `CMakeLists.txt`:

```cmake
find_package(cURLio 0.6 REQUIRED)
target_link_libraries(my-target PRIVATE cURLio::cURLio)
```

## Debugging

To enable logging output compile your executable with the definition `CURLIO_ENABLE_LOGGING`.
