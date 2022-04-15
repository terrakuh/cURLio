
#include <boost/asio/experimental/as_tuple.hpp>
#include <boost/json/src.hpp>
#include <curlio/curlio.hpp>
#include <iostream>

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
			  session.set_cookie_file("/tmp/cookme");
			  printf("session: %p\n", &session);
			  for (int i = 0; i < 2; ++i) {
				  curlio::Request req{};
				  req.set_url("https://git.ayar.eu");
				  curl_easy_setopt(req.native_handle(), CURLOPT_VERBOSE, 1L);
				  // curl_easy_setopt(req.native_handle(), CURLOPT_USERAGENT, "curl/7.80.0");
				  // curl_easy_setopt(req.native_handle(), CURLOPT_COOKIEFILE, "/tmp/cookme");
				  // curl_easy_setopt(req.native_handle(), CURLOPT_COOKIEJAR, "/tmp/cookme");

				  session.start(req);

				  std::cout << co_await curlio::quick::async_read_all(req, use_awaitable);

				  // while (true) {
				  //   char buf[4096];
				  //   auto [ec, n] = co_await req.async_read_some(buffer(buf), use_nothrow_awaitable);
				  //   if (ec == error::eof) {
				  // 	  break;
				  //   }
				  //   // std::cout.write(buf, n);
				  // }
				  co_await req.async_wait(use_awaitable);
				  break;
			  }
		  } catch (const std::exception& e) {
			  std::cerr << "Exception: " << e.what() << "\n";
		  }
		  co_return;
	  },
	  detached);

	service.run();
	curl_global_cleanup();
}
