// #define CURLIO_ENABLE_LOGGING
// #define BOOST_ASIO_ENABLE_HANDLER_TRACKING

#include <curlio/curlio.hpp>
#include <iostream>
#include <numeric>
#include <tuple>

using namespace boost::asio;

awaitable<void> async_main()
try {
	auto session = curlio::make_session(co_await this_coro::executor);
	auto request = curlio::make_request(session);

	std::shared_ptr<curlio::Response> response{};
	for (const auto url : { "http://example.com", "http://google.de" }) {
		request->set_option<CURLOPT_URL>(url);
		response = co_await session->async_start(request, use_awaitable);
		co_await response->async_wait_headers(use_awaitable);

		std::uint64_t checksum = 0;
		std::uint8_t data[4096]{};
		while (true) {
			const auto [ec, bytes_transferred] =
			  co_await response->async_read_some(buffer(data), as_tuple(use_awaitable));
			if (ec) {
				std::cout << "Error: " << ec.message() << "\n";
				break;
			}
			std::cout.write(reinterpret_cast<const char*>(data), bytes_transferred);
			checksum = std::accumulate(data, data + bytes_transferred, checksum);
		}
		std::cout << "Checksum for '" << url << "' is " << checksum << "\n";
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
