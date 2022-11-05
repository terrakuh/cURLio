// #define CURLIO_ENABLE_LOGGING
// #define BOOST_ASIO_ENABLE_HANDLER_TRACKING

#include <boost/asio/experimental/as_tuple.hpp>
#include <boost/crc.hpp>
#include <boost/json/src.hpp>
#include <curlio/curlio.hpp>
#include <fstream>
#include <iostream>
#include <random>
#include <thread>
#include <tuple>

using namespace boost::asio;

constexpr auto use_nothrow_awaitable = experimental::as_tuple(use_awaitable);

std::tuple<std::string, unsigned int, std::size_t> map[] = {
	std::make_tuple("HUAWEI_MateBook_D_16_WDT_1.0.3.11.zip", 2454097580, 162351),
	std::make_tuple("ml.remawmom.2022_cmx1100a_d_owner_s_manual.pdf", 3038542098, 5534135),
	std::make_tuple("virtio-win-guest-tools.exe", 1758795416, 31137536),
};

int main(int argc, char** argv)
{
	curl_global_init(CURL_GLOBAL_ALL);
	io_service service;

	auto session = curlio::make_session<boost::asio::any_io_executor>(service.get_executor());

	std::default_random_engine engine{ 12223 };
	std::uniform_int_distribution<int> dist{ 0, 2 };
	for (int j = 0; j < 2000; ++j) {
		co_spawn(
		  service,
		  [&, i = dist(engine)]() -> awaitable<void> {
			  int k        = j % 3;
			  auto request = curlio::make_request(session);

			  const auto& [name, checksum, size] = map[i];
			  curl_easy_setopt(request->native_handle(), CURLOPT_URL, ("http://localhost:8088/" + name).c_str());

			  auto response = co_await session->async_start(request, use_awaitable);

			  co_await response->async_wait_headers(use_awaitable);

			  char data[1024 * 16];
			  boost::crc_32_type crc{};
			  std::size_t total = 0;
			  while (true) {
				  const auto [ec, read] = co_await response->async_read_some(buffer(data), use_nothrow_awaitable);
				  if (ec) {
					  if (ec != error::eof) {
						  puts(ec.message().c_str());
					  }
					  break;
				  }
				  total += read;
				  crc.process_bytes(data, read);
			  }
			  if (crc.checksum() != checksum) {
				  std::cout << "Checksum not identical for " << name << " (" << total << "/" << size << ")"
				            << "\n";
			  }
			  co_return;
		  },
		  detached);
	}

	std::cout << "Service running with cURL version: " << curl_version() << "\n";
	std::thread t{ [&] { service.run(); } };
	// boost::asio::signal_set sig{service, SIGINT};
	// sig.async_wait([&](auto, auto) {
	// 	service.stop();
	// });
	service.run();
	t.join();
	curl_global_cleanup();
}
