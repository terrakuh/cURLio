#pragma once

#include "contains.hpp"

#include <curl/curl.h>
#include <type_traits>

namespace curlio::detail {

template<CURLINFO Option, typename = void>
struct Info_type;

template<CURLINFO Option>
struct Info_type<Option, std::enable_if_t<(Option & CURLINFO_TYPEMASK) == CURLINFO_STRING>> {
	using type = const char*;
};

template<CURLINFO Option>
struct Info_type<Option, std::enable_if_t<(Option & CURLINFO_TYPEMASK) == CURLINFO_LONG>> {
	using type = long;
};

template<CURLINFO Option>
struct Info_type<Option, std::enable_if_t<(Option & CURLINFO_TYPEMASK) == CURLINFO_DOUBLE>> {
	using type = double;
};

template<CURLINFO Option>
struct Info_type<Option, std::enable_if_t<(Option & CURLINFO_TYPEMASK) == CURLINFO_OFF_T>> {
	using type = curl_off_t;
};

template<CURLINFO Option>
using info_type = typename Info_type<Option>::type;

template<CURLoption Option, typename = void>
struct Option_type;

template<CURLoption Option>
struct Option_type<Option,
                   std::enable_if_t<(Option > CURLOPTTYPE_LONG && Option < CURLOPTTYPE_LONG + 10'000)>> {
	using type = long;
};

template<CURLoption Option>
struct Option_type<
  Option,
  std::enable_if_t<contains<
    Option, CURLOPT_URL, CURLOPT_PROXY, CURLOPT_USERPWD, CURLOPT_PROXYUSERPWD, CURLOPT_RANGE, CURLOPT_POSTFIELDS, CURLOPT_REFERER,
    CURLOPT_FTPPORT, CURLOPT_USERAGENT, CURLOPT_COOKIE, CURLOPT_SSLCERT, CURLOPT_KEYPASSWD,
    CURLOPT_COOKIEFILE, CURLOPT_CUSTOMREQUEST, CURLOPT_INTERFACE, CURLOPT_KRBLEVEL, CURLOPT_CAINFO,
    CURLOPT_RANDOM_FILE, CURLOPT_EGDSOCKET, CURLOPT_COOKIEJAR, CURLOPT_SSL_CIPHER_LIST, CURLOPT_SSLCERTTYPE,
    CURLOPT_SSLKEY, CURLOPT_SSLKEYTYPE, CURLOPT_SSLENGINE, CURLOPT_CAPATH, CURLOPT_ACCEPT_ENCODING,
    CURLOPT_NETRC_FILE, CURLOPT_FTP_ACCOUNT, CURLOPT_COOKIELIST, CURLOPT_FTP_ALTERNATIVE_TO_USER,
    CURLOPT_SSH_PUBLIC_KEYFILE, CURLOPT_SSH_PRIVATE_KEYFILE, CURLOPT_SSH_HOST_PUBLIC_KEY_MD5, CURLOPT_COPYPOSTFIELDS, CURLOPT_CRLFILE,
    CURLOPT_ISSUERCERT, CURLOPT_USERNAME, CURLOPT_PASSWORD, CURLOPT_PROXYUSERNAME, CURLOPT_PROXYPASSWORD,
    CURLOPT_NOPROXY, CURLOPT_SOCKS5_GSSAPI_SERVICE, CURLOPT_SSH_KNOWNHOSTS, CURLOPT_MAIL_FROM,
    CURLOPT_RTSP_SESSION_ID, CURLOPT_RTSP_STREAM_URI, CURLOPT_RTSP_TRANSPORT, CURLOPT_TLSAUTH_USERNAME,
    CURLOPT_TLSAUTH_PASSWORD, CURLOPT_TLSAUTH_TYPE, CURLOPT_DNS_SERVERS, CURLOPT_MAIL_AUTH,
    CURLOPT_XOAUTH2_BEARER, CURLOPT_DNS_INTERFACE, CURLOPT_DNS_LOCAL_IP4, CURLOPT_DNS_LOCAL_IP6,
    CURLOPT_LOGIN_OPTIONS, CURLOPT_PINNEDPUBLICKEY, CURLOPT_UNIX_SOCKET_PATH, CURLOPT_PROXY_SERVICE_NAME,
    CURLOPT_SERVICE_NAME, CURLOPT_DEFAULT_PROTOCOL, CURLOPT_PROXY_CAINFO, CURLOPT_PROXY_CAPATH,
    CURLOPT_PROXY_TLSAUTH_USERNAME, CURLOPT_PROXY_TLSAUTH_PASSWORD, CURLOPT_PROXY_TLSAUTH_TYPE,
    CURLOPT_PROXY_SSLCERT, CURLOPT_PROXY_SSLCERTTYPE, CURLOPT_PROXY_SSLKEY, CURLOPT_PROXY_SSLKEYTYPE,
    CURLOPT_PROXY_KEYPASSWD, CURLOPT_PROXY_SSL_CIPHER_LIST, CURLOPT_PROXY_CRLFILE, CURLOPT_PRE_PROXY,
    CURLOPT_PROXY_PINNEDPUBLICKEY, CURLOPT_ABSTRACT_UNIX_SOCKET, CURLOPT_REQUEST_TARGET,
    CURLOPT_TLS13_CIPHERS, CURLOPT_PROXY_TLS13_CIPHERS, CURLOPT_DOH_URL, CURLOPT_ALTSVC, CURLOPT_SASL_AUTHZID,
    CURLOPT_PROXY_ISSUERCERT, CURLOPT_SSL_EC_CURVES, CURLOPT_HSTS, CURLOPT_AWS_SIGV4,
    CURLOPT_SSH_HOST_PUBLIC_KEY_SHA256>>> {
	using type = const char*;
};

template<CURLoption Option>
struct Option_type<
  Option,
  std::enable_if_t<contains<Option, CURLOPT_HTTPHEADER, CURLOPT_QUOTE, CURLOPT_POSTQUOTE,
                            CURLOPT_TELNETOPTIONS, CURLOPT_PREQUOTE, CURLOPT_HTTP200ALIASES,
                            CURLOPT_MAIL_RCPT, CURLOPT_RESOLVE, CURLOPT_PROXYHEADER, CURLOPT_CONNECT_TO>>> {
	using type = curl_slist*;
};

template<CURLoption Option>
struct Option_type<
  Option,
  std::enable_if_t<contains<Option, CURLOPT_WRITEFUNCTION, CURLOPT_READFUNCTION, CURLOPT_HEADERFUNCTION>>> {
	using type = size_t (*)(char*, size_t, size_t, void*);
};

template<CURLoption Option>
struct Option_type<Option, std::enable_if_t<contains<Option, CURLOPT_PROGRESSFUNCTION>>> {
	using type = int (*)(void*, double, double, double, double);
};

template<CURLoption Option>
struct Option_type<Option, std::enable_if_t<contains<Option, CURLOPT_DEBUGFUNCTION>>> {
	using type = int (*)(CURL*, curl_infotype, char*, size_t, void*);
};

template<CURLoption Option>
struct Option_type<Option, std::enable_if_t<contains<Option, CURLOPT_SSL_CTX_FUNCTION>>> {
	using type = CURLcode (*)(CURL*, void*, void*);
};

template<CURLoption Option>
struct Option_type<Option, std::enable_if_t<contains<Option, CURLOPT_IOCTLFUNCTION>>> {
	using type = curlioerr (*)(CURL*, int, void*);
};

template<CURLoption Option>
struct Option_type<
  Option, std::enable_if_t<contains<Option, CURLOPT_CONV_FROM_NETWORK_FUNCTION,
                                    CURLOPT_CONV_TO_NETWORK_FUNCTION, CURLOPT_CONV_FROM_UTF8_FUNCTION>>> {
	using type = CURLcode(char*, size_t);
};

template<CURLoption Option>
struct Option_type<Option, std::enable_if_t<contains<Option, CURLOPT_SOCKOPTFUNCTION>>> {
	using type = int(void*, curl_socket_t, curlsocktype);
};

template<CURLoption Option>
struct Option_type<Option, std::enable_if_t<contains<Option, CURLOPT_OPENSOCKETFUNCTION>>> {
	using type = curl_socket_t(void*, curlsocktype, curl_sockaddr*);
};

template<CURLoption Option>
struct Option_type<Option, std::enable_if_t<contains<Option, CURLOPT_SEEKFUNCTION>>> {
	using type = int(void*, curl_off_t, int);
};

template<CURLoption Option>
struct Option_type<Option, std::enable_if_t<contains<Option, CURLOPT_SSH_KEYFUNCTION>>> {
	using type = int(CURL*, const curl_khkey*, const curl_khkey*, curl_khmatch, void*);
};

template<CURLoption Option>
struct Option_type<Option, std::enable_if_t<contains<Option, CURLOPT_INTERLEAVEFUNCTION>>> {
	using type = size_t(void*, size_t, size_t, void*);
};

template<CURLoption Option>
struct Option_type<Option, std::enable_if_t<contains<Option, CURLOPT_CHUNK_BGN_FUNCTION>>> {
	using type = long(const void*, void*, int);
};

template<CURLoption Option>
struct Option_type<Option, std::enable_if_t<contains<Option, CURLOPT_CHUNK_END_FUNCTION>>> {
	using type = long(void*);
};

template<CURLoption Option>
struct Option_type<Option, std::enable_if_t<contains<Option, CURLOPT_FNMATCH_FUNCTION>>> {
	using type = int(void*, const char*, const char*);
};

template<CURLoption Option>
struct Option_type<Option, std::enable_if_t<contains<Option, CURLOPT_CLOSESOCKETFUNCTION>>> {
	using type = int(void*, curl_socket_t);
};

template<CURLoption Option>
struct Option_type<Option, std::enable_if_t<contains<Option, CURLOPT_XFERINFOFUNCTION>>> {
	using type = int(void*, curl_off_t, curl_off_t, curl_off_t, curl_off_t);
};

template<CURLoption Option>
struct Option_type<Option, std::enable_if_t<contains<Option, CURLOPT_RESOLVER_START_FUNCTION>>> {
	using type = int(void*, void*, void*);
};

template<CURLoption Option>
struct Option_type<Option, std::enable_if_t<contains<Option, CURLOPT_TRAILERFUNCTION>>> {
	using type = int(curl_slist**, void*);
};

template<CURLoption Option>
struct Option_type<Option, std::enable_if_t<contains<Option, CURLOPT_HSTSREADFUNCTION>>> {
	using type = CURLSTScode(CURL*, curl_hstsentry*, void*);
};

template<CURLoption Option>
struct Option_type<Option, std::enable_if_t<contains<Option, CURLOPT_HSTSWRITEFUNCTION>>> {
	using type = CURLSTScode(CURL*, curl_hstsentry*, curl_index*, void*);
};

template<CURLoption Option>
struct Option_type<Option, std::enable_if_t<contains<Option, CURLOPT_PREREQFUNCTION>>> {
	using type = int(void*, char*, char*, int, int);
};

template<CURLoption Option>
struct Option_type<
  Option, std::enable_if_t<contains<
            Option, CURLOPT_WRITEDATA, CURLOPT_READDATA, CURLOPT_HEADERDATA, CURLOPT_XFERINFODATA,
            CURLOPT_DEBUGDATA, CURLOPT_SSL_CTX_DATA, CURLOPT_IOCTLDATA, CURLOPT_SOCKOPTDATA,
            CURLOPT_OPENSOCKETDATA, CURLOPT_SEEKDATA, CURLOPT_SSH_KEYDATA, CURLOPT_INTERLEAVEDATA,
            CURLOPT_CHUNK_DATA, CURLOPT_FNMATCH_DATA, CURLOPT_CLOSESOCKETDATA, CURLOPT_RESOLVER_START_DATA,
            CURLOPT_TRAILERDATA, CURLOPT_HSTSREADDATA, CURLOPT_HSTSWRITEDATA, CURLOPT_PREREQDATA>>> {
	using type = void*;
};

template<CURLoption Option>
struct Option_type<Option,
                   std::enable_if_t<(Option > CURLOPTTYPE_OFF_T && Option < CURLOPTTYPE_OFF_T + 10'000)>> {
	using type = curl_off_t;
};

template<CURLoption Option>
struct Option_type<Option,
                   std::enable_if_t<(Option > CURLOPTTYPE_BLOB && Option < CURLOPTTYPE_BLOB + 10'000)>> {
	using type = curl_blob*;
};

template<CURLoption Option>
using option_type = typename Option_type<Option>::type;

} // namespace curlio::detail
