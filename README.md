# cURLio
Simple header-only **Boost.ASIO** wrapper.

## Example

```cpp
curlio::Session session{ service.get_executor() };
curlio::Request req;
req.set_url("https://example.com");
curl_easy_setopt(req.native_handle(), CURLOPT_USERAGENT, "curl/7.80.0");

session.start(req);
while (true) {
	char buf[4096];
	auto [ec, n] = co_await req.async_read_some(buffer(buf), use_nothrow_awaitable);
	if (ec == error::eof) {
		break;
	}
	std::cout.write(buf, n);
}
co_await req.async_wait(use_awaitable);
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
find_package(curlio REQUIRED)

target_link_libraries(my-target PRIVATE curlio::curlio)
```
