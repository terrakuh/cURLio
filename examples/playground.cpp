
#include <boost/asio/experimental/as_tuple.hpp>
#include <curlio/session.hpp>
#include <iostream>

using namespace boost::asio;

constexpr auto use_nothrow_awaitable = experimental::as_tuple(use_awaitable);

int main(int argc, char** argv)
{
	std::string s;
	io_service service;

	co_spawn(
	  service,
	  [&]() -> awaitable<void> {
		  curlio::Session session{ service.get_executor() };
		  curlio::Request req{};
		  req.set_url("https://example.com");
		  // curl_easy_setopt(req.native_handle(), CURLOPT_VERBOSE, 1L);
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
	  },
	  detached);

	service.run();
}
