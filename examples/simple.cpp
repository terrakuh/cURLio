/**
 * @example
 */
// #define CURLIO_USE_STANDALONE_ASIO
// #define CURLIO_ENABLE_LOGGING

#include <cURLio.hpp>
#include <iostream>

using namespace CURLIO_ASIO_NS;

int main(int argc, char** argv)
{
	curl_global_init(CURL_GLOBAL_ALL);
	io_service service;

	cURLio::Session session{ service.get_executor() };
	co_spawn(
	  service,
	  [&]() -> awaitable<void> {
		  // Create request and set options.
		  auto request = std::make_shared<cURLio::Request>(session);
		  curl_easy_setopt(request->native_handle(), CURLOPT_URL, "http://example.com");
		  curl_easy_setopt(request->native_handle(), CURLOPT_USERAGENT, "cURLio");

		  // Launches the request which will then run in the background.
		  auto response = co_await session.async_start(request, use_awaitable);

		  co_await response->async_wait_headers(use_awaitable);

		  // Read all and do something with the data.
		  char data[4096];
		  while (true) {
			  try {
				  const auto bytes_transferred = co_await response->async_read_some(buffer(data), use_awaitable);
				  /* Do something. */
				  std::cout.write(data, bytes_transferred);
			  } catch (...) {
				  break;
			  }
		  }

		  // When all data was read and the `response->async_read_some()` returned `asio::error::eof`
		  // the request is removed from the session and can be used again.
		  std::cout << "All transfers done\n";
	  },
	  detached);

	service.run();
	curl_global_cleanup();
}
