
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
		  curlio::Session session{ service.get_executor() };
		  curlio::Request req{};
		  req.set_url("http://localhost:8080");
		  // curl_easy_setopt(req.native_handle(), CURLOPT_VERBOSE, 1L);
		  curl_easy_setopt(req.native_handle(), CURLOPT_USERAGENT, "curl/7.80.0");
		  curl_easy_setopt(req.native_handle(), CURLOPT_HEADERFUNCTION, &header_callback);
		  curl_easy_setopt(req.native_handle(), CURLOPT_POST, 1);

		  session.start(req);
		  boost::json::object payload;
		  payload["user"]     = "kartoffel";
		  payload["password"] = "secret";

		  try {
			  co_await curlio::async_write_json(req, payload, use_nothrow_awaitable);
		  } catch (const std::exception& e) {
			  std::cerr << "Error: " << e.what() << "\n";
			  co_return;
		  }
		  co_await req.async_write_some(null_buffers{}, use_awaitable);

		  while (true) {
			  char buf[4096];
			  auto [ec, n] = co_await req.async_read_some(buffer(buf), use_nothrow_awaitable);
			  if (ec == error::eof) {
				  break;
			  }
			  // std::cout.write(buf, n);
		  }
		  co_await req.async_wait(use_awaitable);
		  co_return;
	  },
	  detached);

	service.run();
	curl_global_cleanup();
}
