#pragma once

#include <curl/curl.h>
#include <shared_mutex>
#include <utility>

namespace curlio::detail {

class CURL_share_lock {
public:
	void lock(CURL* handle, curl_lock_data data, curl_lock_access access, void* self_pointer) noexcept
	{
		// const auto self = static_cast<CURL_share_lock*>(self_pointer);
		// Mutex* mutex    = nullptr;
		// printf("locking %p for %s as %s\n", self_pointer,
		//        data == CURL_LOCK_DATA_COOKIE ? "CURL_LOCK_DATA_COOKIE" : "CURL_LOCK_DATA_DNS",
		//        access == CURL_LOCK_ACCESS_SHARED ? "shared" : "excluse");
		// switch (data) {
		// case CURL_LOCK_DATA_COOKIE: mutex = &self->_cookie_mutex; break;
		// case CURL_LOCK_DATA_DNS: mutex = &self->_dns_mutex; break;
		// }
		// if (mutex != nullptr) {
		// 	if (access == CURL_LOCK_ACCESS_SHARED) {
		// 		mutex->second.lock_shared();
		// 		// does not need to be synchronized
		// 		mutex->first = true;
		// 	} else {
		// 		mutex->second.lock();
		// 		mutex->first = false;
		// 	}
		// }
	}
	void unlock(CURL* handle, curl_lock_data data, void* self_pointer) noexcept
	{
		// const auto self = static_cast<CURL_share_lock*>(self_pointer);
		// Mutex* mutex    = nullptr;
		// printf("%p %p %p \n", handle, data,  self_pointer);
		// printf("unlocking %p for %s\n", self_pointer,
		//        data == CURL_LOCK_DATA_COOKIE ? "CURL_LOCK_DATA_COOKIE" : "CURL_LOCK_DATA_DNS");
		// switch (data) {
		// case CURL_LOCK_DATA_COOKIE: mutex = &self->_cookie_mutex; break;
		// case CURL_LOCK_DATA_DNS: mutex = &self->_dns_mutex; break;
		// }
		// if (mutex != nullptr) {
		// 	if (mutex->first) {
		// 		mutex->second.unlock_shared();
		// 	} else {
		// 		mutex->second.unlock();
		// 	}
		// }
	}

private:
	typedef std::pair<bool, std::shared_mutex> Mutex;

	Mutex _cookie_mutex;
	Mutex _dns_mutex;
};

} // namespace curlio::detail
