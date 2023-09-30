file(GLOB_RECURSE sources "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp")

add_library(curlio INTERFACE ${sources})
add_library(curlio::curlio ALIAS curlio)
target_link_libraries(curlio INTERFACE CURL::libcurl Threads::Threads Boost::boost)
target_include_directories(
  curlio INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>" $<INSTALL_INTERFACE:include>
)

install(TARGETS curlio EXPORT ${PROJECT_NAME}-targets)
install(
  DIRECTORY .
  DESTINATION include/${PROJECT_NAME}
  FILES_MATCHING
  PATTERN "*.hpp"
  PATTERN "*.inl"
)
