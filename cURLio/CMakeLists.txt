file(GLOB_RECURSE hpp_sources "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")
file(GLOB_RECURSE inl_sources "${CMAKE_CURRENT_SOURCE_DIR}/*.inl")

add_library(cURLio INTERFACE ${hpp_sources} ${inl_sources} ../cURLio.hpp)
add_library(cURLio::cURLio ALIAS cURLio)
target_link_libraries(
  cURLio INTERFACE CURL::libcurl Threads::Threads
                   $<IF:$<BOOL:CURLIO_USE_STANDALONE_ASIO>,asio::asio,Boost::boost>
)
target_include_directories(
  cURLio INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>" $<INSTALL_INTERFACE:include>
)
target_compile_features(cURLio INTERFACE cxx_std_17)

if(CURLIO_ENABLE_LOGGING)
  target_compile_definitions(cURLio INTERFACE CURLIO_ENABLE_LOGGING)
endif()

install(TARGETS cURLio EXPORT ${PROJECT_NAME}-targets)
