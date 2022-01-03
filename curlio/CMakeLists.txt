file(GLOB_RECURSE sources "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp")

add_library(curlio INTERFACE ${sources})
add_library(curlio::curlio ALIAS curlio)
target_link_libraries(curlio INTERFACE CURL::libcurl Threads::Threads Boost::boost)
target_include_directories(curlio BEFORE INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>")

install(TARGETS curlio EXPORT ${PROJECT_NAME}-targets)
install(
  EXPORT ${PROJECT_NAME}-targets
  DESTINATION lib/cmake/${PROJECT_NAME}
  NAMESPACE ${PROJECT_NAME}::
  EXPORT_LINK_INTERFACE_LIBRARIES
)
install(
  DIRECTORY .
  DESTINATION include/${PROJECT_NAME}
  FILES_MATCHING
  PATTERN "*.hpp"
)
