#define CURLIO_ENABLE_LOGGING

#include <boost/asio/experimental/as_tuple.hpp>
#include <boost/json/src.hpp>
#include <curlio/curlio.hpp>
#include <iostream>
#include <thread>

using namespace boost::asio;

constexpr auto use_nothrow_awaitable = experimental::as_tuple(use_awaitable);

inline size_t header_callback(char* buffer, size_t size, size_t nitems, void* userdata)
{
	printf("header: ");
	fwrite(buffer, size, nitems, stdout);
	return size * nitems;
}

int main(int argc, char** argv)
{
	curl_global_init(CURL_GLOBAL_ALL);
	std::string s;
	io_service service;

	co_spawn(
	  service,
	  [&]() -> awaitable<void> {
		  try {
			  curlio::Session session{ service.get_executor() };
			  curlio::Request req{};
			  req.set_url("http://localhost:8083/kartoffel.bin");
			  auto resp = session.start(req);

			  // steady_timer timer{service};
			  // timer.expires_after(std::chrono::minutes{3});
			  // co_await timer.async_wait(use_awaitable);
			  // std::cout << "Done with artifical timeout\n";

			  do {
				  co_await resp.async_await_next_headers(use_awaitable);
				  std::cout << "=======RECEIVED HEADER======\n";
			  } while (resp.is_redirect());
			  std::cout << "Final headers received\n";

			  // co_await curlio::quick::async_read_all(resp, use_awaitable);
			  // std::cout << "\nRead all data\n";

			  while (true) {
				  char buf[4096];
				  auto [ec, n] = co_await resp.async_read_some(buffer(buf), use_nothrow_awaitable);
				  if (ec == error::eof) {
					  break;
				  }
				  // std::cout.write(buf, n);
			  }
				std::cout << "Done reading\n";
			  co_await resp.async_await_completion(use_awaitable);
		  } catch (const std::exception& e) {
			  std::cerr << "Exception: " << e.what() << "\n";
		  }
		  co_return;
	  },
	  detached);

	std::cout << "Service running with cURL version: " << curl_version() << "\n";
	std::thread t{ [&] { service.run(); } };
	service.run();
	t.join();
	curl_global_cleanup();
}
