file(GLOB_RECURSE hpp_sources "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")
file(GLOB_RECURSE inl_sources "${CMAKE_CURRENT_SOURCE_DIR}/*.inl")

foreach(suffix asio boost-asio)
  add_library(cURLio-${suffix} INTERFACE ${hpp_sources} ${inl_sources} ../cURLio.hpp)
  add_library(cURLio::cURLio-${suffix} ALIAS cURLio-${suffix})
  target_link_libraries(cURLio-${suffix} INTERFACE CURL::libcurl Threads::Threads)
  if(suffix STREQUAL "asio")
    target_link_libraries(cURLio-${suffix} INTERFACE asio::asio)
    target_compile_definitions(cURLio-${suffix} INTERFACE CURLIO_USE_STANDALONE_ASIO)
  else()
    target_link_libraries(cURLio-${suffix} INTERFACE Boost::boost)
  endif()
  target_include_directories(
    cURLio-${suffix} INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
                               $<INSTALL_INTERFACE:include>
  )
  target_compile_features(cURLio-${suffix} INTERFACE cxx_std_17)

  if(CURLIO_ENABLE_LOGGING)
    target_compile_definitions(cURLio-${suffix} INTERFACE CURLIO_ENABLE_LOGGING)
  endif()

  install(TARGETS cURLio-${suffix} EXPORT ${PROJECT_NAME}-targets)
endforeach()

if(CURLIO_USE_STANDALONE_ASIO)
  add_library(cURLio::cURLio ALIAS cURLio-asio)
else()
  add_library(cURLio::cURLio ALIAS cURLio-boost-asio)
endif()
