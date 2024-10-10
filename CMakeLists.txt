cmake_minimum_required(VERSION 3.16)

project(
  cURLio
  VERSION 0.6.0
  DESCRIPTION "The simple glue for cURL and Boost.ASIO"
  HOMEPAGE_URL "https://github.com/terrakuh/cURLio"
  LANGUAGES CXX
)

set(CURLIO_TOP_LEVEL TRUE)
get_directory_property(parent_directory PARENT_DIRECTORY)
if(parent_directory)
  set(CURLIO_TOP_LEVEL FALSE)
endif()

option(CURLIO_BUILD_EXAMPLES "The example programs." ${CURLIO_TOP_LEVEL})
option(CURLIO_ENABLE_LOGGING "Prints debug logs during execution." OFF)
option(CURLIO_USE_STANDALONE_ASIO "Use the standalone ASIO library." OFF)
mark_as_advanced(CURLIO_ENABLE_LOGGING)

find_package(CURL 7.21 REQUIRED)
find_package(Threads REQUIRED)

if(NOT CURLIO_USE_STANDALONE_ASIO)
  find_package(Boost 1.78 REQUIRED)
elseif(NOT TARGET asio::asio)
  message(FATAL_ERROR "Standalone ASIO mode requires the target asio::asio")
endif()

add_subdirectory(cURLio)

if(CURLIO_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

# Install
set(INCLUDE_INSTALL_DIR "include/")
set(LIBRARY_INSTALL_DIR "lib/${PROJECT_NAME}")

include(CMakePackageConfigHelpers)
configure_package_config_file(
  ${PROJECT_NAME}Config.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
  INSTALL_DESTINATION "${LIBRARY_INSTALL_DIR}/cmake/${PROJECT_NAME}"
  PATH_VARS INCLUDE_INSTALL_DIR LIBRARY_INSTALL_DIR
)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
              "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
        DESTINATION "${LIBRARY_INSTALL_DIR}/cmake"
)

install(
  DIRECTORY ${PROJECT_NAME}
  DESTINATION "${INCLUDE_INSTALL_DIR}"
  FILES_MATCHING
  PATTERN "*.hpp"
  PATTERN "*.inl"
)
install(FILES cURLio.hpp DESTINATION "${INCLUDE_INSTALL_DIR}")
install(
  EXPORT ${PROJECT_NAME}-targets
  DESTINATION "${LIBRARY_INSTALL_DIR}/cmake"
  NAMESPACE ${PROJECT_NAME}::
  EXPORT_LINK_INTERFACE_LIBRARIES
)

# CPack
if(CURLIO_TOP_LEVEL)
  set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
  set(CPACK_PACKAGE_CONTACT "Yunus Ayar")
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
  set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}")
  set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
  set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
  set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
  set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
  set(CPACK_GENERATOR DEB TGZ)
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "")
  set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

  include(CPack)
endif()
