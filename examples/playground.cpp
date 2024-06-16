// #define CURLIO_ENABLE_LOGGING
// #define BOOST_ASIO_ENABLE_HANDLER_TRACKING

#include <curlio/curlio.hpp>
#include <iostream>
#include <numeric>
#include <tuple>
#include <fstream>

using namespace boost::asio;

awaitable<void> async_main()
try {
	auto session = curlio::make_session(co_await this_coro::executor);
	auto request = curlio::make_request(session);

	request->set_option<CURLOPT_URL>("http://127.0.0.1:8088/");
	request->set_option<CURLOPT_MAXREDIRS>(3);
	request->set_option<CURLOPT_FOLLOWLOCATION>(1);

	auto response = co_await session->async_start(request, use_awaitable);

	std::ofstream file{"/workspaces/downer/backend/asd", std::ios::out|std::ios::binary};
	char data[10*1024];
	while (true) {
		const std::size_t bytes_transferred = co_await response->async_read_some(buffer(data), use_awaitable);
		file.write(data, bytes_transferred);
	}
} catch (const std::exception& e) {
	std::cerr << "Failed with: " << e.what() << "\n";
}

int main(int argc, char** argv)
{
	curl_global_init(CURL_GLOBAL_ALL);
	io_service service{};

	auto session = curlio::make_session<boost::asio::any_io_executor>(service.get_executor());

	co_spawn(service, async_main(), detached);

	std::cout << "Service running with cURL version: " << curl_version() << "\n";
	service.run();
	curl_global_cleanup();
}
