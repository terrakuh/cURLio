file(GLOB_RECURSE sources "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp")

add_library(cURLio INTERFACE ${sources})
add_library(cURLio::cURLio ALIAS cURLio)
target_link_libraries(cURLio INTERFACE CURL::libcurl Threads::Threads Boost::boost)
target_include_directories(
  cURLio INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>" $<INSTALL_INTERFACE:include>
)
target_compile_features(cURLio INTERFACE cxx_std_17)

if(CURLIO_ENABLE_LOGGING)
  target_compile_definitions(cURLio INTERFACE CURLIO_ENABLE_LOGGING)
endif()

install(TARGETS cURLio EXPORT ${PROJECT_NAME}-targets)
install(
  DIRECTORY .
  DESTINATION include/${PROJECT_NAME}
  FILES_MATCHING
  PATTERN "*.hpp"
  PATTERN "*.inl"
)
