file(GLOB_RECURSE sources "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp")

add_library(curlio INTERFACE ${sources})
add_library(curlio::curlio ALIAS curlio)
target_link_libraries(curlio INTERFACE CURL::libcurl Threads::Threads ${asio_target})
target_include_directories(
  curlio INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
                   $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(curlio INTERFACE cxx_std_17)

if(CURLIO_USE_STANDALONE_ASIO)
  target_compile_definitions(curlio INTERFACE CURLIO_USE_STANDALONE_ASIO)
endif()

install(TARGETS curlio EXPORT ${PROJECT_NAME}-targets)
install(
  DIRECTORY .
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}
  FILES_MATCHING
  PATTERN "*.hpp"
)
