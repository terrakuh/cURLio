// #define CURLIO_ENABLE_LOGGING
// #define BOOST_ASIO_ENABLE_HANDLER_TRACKING

#include <curlio/curlio.hpp>
#include <iostream>

using namespace boost::asio;

awaitable<curlio::Headers> await_headers(curlio::Response& response, unsigned int max_redirects)
{
	for (unsigned int i = 0; true; ++i) {
		auto headers     = co_await response.async_wait_headers(use_awaitable);
		const int status = co_await response.async_get_info<CURLINFO_RESPONSE_CODE>(use_awaitable);
		if (status < 300 || status >= 400) {
			std::cout << "Final headers received\n";
			co_return headers;
		} else if (i == max_redirects) {
			break;
		}
		std::cout << "Waiting for next header: " << headers.size() << " status: " << status << "\n";
	}
	throw std::runtime_error{ "max redirects" };
}

awaitable<void> async_main()
{
	curlio::Session session{ co_await this_coro::executor };

	auto request = std::make_shared<curlio::Request>(session);
	request->set_option<CURLOPT_URL>("http://localhost:8088");
	request->set_option<CURLOPT_MAXREDIRS>(3);
	request->set_option<CURLOPT_FOLLOWLOCATION>(1);

	std::shared_ptr<curlio::Response> response{};

	for (int i = 0; i < 2; ++i) {
		response           = co_await session.async_start(request, use_awaitable);
		const auto headers = co_await await_headers(*response, 3);
		const int status   = co_await response->async_get_info<CURLINFO_RESPONSE_CODE>(use_awaitable);
		std::cout << "Got " << headers.size() << " headers\n";
		std::cout << "Status: " << status << "\n";

		std::size_t bytes_transferred = 0;
		try {
			char data[10 * 1024];
			while (true) {
				bytes_transferred += co_await response->async_read_some(buffer(data), use_awaitable);
			}
		} catch (const std::exception& e) {
		}
		std::cout << "Finished with " << bytes_transferred << " bytes\n";
	}
}

int main(int argc, char** argv)
{
	curl_global_init(CURL_GLOBAL_ALL);
	io_service service{};

	co_spawn(service, async_main(), detached);

	std::cout << "Service running with cURL version: " << curl_version() << "\n";
	service.run();
	curl_global_cleanup();
}
