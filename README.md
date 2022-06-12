# cURLio

Simple C++ 14 header-only **Boost.ASIO** wrapper.

## Example

```cpp
curlio::Session session{ service.get_executor() };
curlio::Request req;
req.set_url("https://example.com");
curl_easy_setopt(req.native_handle(), CURLOPT_USERAGENT, "curl/7.80.0");

auto resp = session.start(req);
co_await resp.async_await_last_headers(use_awaitable);
const std::string content = co_await curlio::quick::async_read_all(resp, use_awaitable);
co_await resp.async_await_completion(use_awaitable);
co_return;
```

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
