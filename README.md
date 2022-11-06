# cURLio

The simple C++ 17 glue between [ASIO](https://think-async.com/Asio/) and [cURL](https://curl.se/). The library is fully templated and header-only. It follows the basic principles of the ASIO design and defines `async_` functions that can take different completion handlers.

On the cURL side the [multi_socket](https://everything.curl.dev/libcurl/drive/multi-socket) approach is implemented with the `curlio::Session` class which can handle thousands of requests in parallel. The current implementation of a session is synchronized via ASIO's strand mechanism in order to avoid concurrent access to the cURL handles in the background.

## Example

The following examples uses the [coroutines](https://en.cppreference.com/w/cpp/language/coroutines) which were introduced in C++ 20:

```cpp
asio::io_service service{};

auto session = curlio::make_session<boost::asio::any_io_executor>(service.get_executor());

// Create request and set options.
auto request = curlio::make_request(session);
curl_easy_setopt(request->native_handle(), CURLOPT_URL, "http://example.com");
curl_easy_setopt(request->native_handle(), CURLOPT_USERAGENT, "cURLio");

// Launches the request which will then run in the background.
auto response = co_await session->async_start(request, asio::use_awaitable);

co_await response->async_wait_headers(asio::use_awaitable);

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

Define the macro `CURLIO_USE_STANDALONE_ASIO` before the first inclusion of `<curlio/curlio.hpp>` to switch from Boost.ASIO to the standalone ASIO library.

## Installation

```sh
git clone https://github.com/terrakuh/curlio.git
cmake -S curlio -B curlio/build
cmake --install curlio/build
```

And then in your `CMakeLists.txt`:

```cmake
find_package(curlio 0.3.3 REQUIRED)
target_link_libraries(my-target PRIVATE curlio::curlio)
```

## Debugging

To enable logging output compile your executable with the definition `CURLIO_ENABLE_LOGGING`.
