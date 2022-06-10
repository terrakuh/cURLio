#include <boost/asio.hpp>
#include <boost/json/src.hpp>
#include <curlio/curlio.hpp>
#include <iostream>

using namespace boost::asio;

int main(int argc, char** argv)
{
	curl_global_init(CURL_GLOBAL_ALL);
	std::string s;
	io_service service;

	co_spawn(
	  service,
	  [&]() -> awaitable<void> {
		  curlio::Session session{ service.get_executor() };
		  curlio::Request req{};
		  req.set_url("http://example.com");
		  auto resp = session.start(req);

		  co_await resp.async_await_last_headers(use_awaitable);
		  std::cout << "Headers received\n";

		  const std::string content = co_await curlio::quick::async_read_all(resp, use_awaitable);
		  std::cout << content << "\n";
		  std::cout << "Done reading " << content.length() << " bytes\n";

		  // // or manually
		  // char buf[4096];
		  // while (true) {
		  //   boost::system::error_code ec;
		  //   const std::size_t read =
		  //     co_await resp.async_read_some(buffer(buf), redirect_error(use_awaitable, ec));
		  //   if (ec == error::eof) {
		  // 	  break;
		  //   }
		  //   std::cout.write(buf, read);
		  // }

		  co_await resp.async_await_completion(use_awaitable);
		  co_return;
	  },
	  detached);

	service.run();
	curl_global_cleanup();
}
