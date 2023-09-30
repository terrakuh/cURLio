cmake_minimum_required(VERSION 3.16)

project(
  curlio
  VERSION 0.4.0
  DESCRIPTION "The simple glue for cURL and Boost.ASIO"
  HOMEPAGE_URL "https://github.com/terrakuh/curlio"
  LANGUAGES CXX
)

set(${PROJECT_NAME}_TOP_LEVEL ON)
get_directory_property(parent_directory PARENT_DIRECTORY)
if(parent_directory)
  set(${PROJECT_NAME}_TOP_LEVEL OFF)
endif()

option(CURLIO_BUILD_EXAMPLES "The example programs." ${${PROJECT_NAME}_TOP_LEVEL})
option(CURLIO_FETCH_DEPENDENCIES "Fetches the required dependencies automatically." OFF)

find_package(CURL 7.21 REQUIRED)
find_package(Threads REQUIRED)

set(required "")
if(NOT CURLIO_FETCH_DEPENDENCIES)
  set(required REQUIRED)
endif()

find_package(Boost 1.78 ${required})
if(NOT Boost_FOUND AND CURLIO_FETCH_DEPENDENCIES)
  message(STATUS "Boost not found. Fetching from jfrog.io")
  include(FetchContent)
  FetchContent_Declare(
    Boost
    URL https://boostorg.jfrog.io/artifactory/main/release/1.83.0/source/boost_1_83_0.tar.gz
    URL_HASH SHA256=c0685b68dd44cc46574cce86c4e17c0f611b15e195be9848dfd0769a0a207628
  )
  FetchContent_MakeAvailable(Boost)
  FetchContent_GetProperties(Boost SOURCE_DIR Boost_INCLUDE_DIR)
  find_package(Boost REQUIRED)
endif()

add_subdirectory(curlio)

if(CURLIO_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

# Install
include(CMakePackageConfigHelpers)
configure_package_config_file(
  ${PROJECT_NAME}-config.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake"
  INSTALL_DESTINATION "${LIBRARY_INSTALL_DIR}/cmake/${PROJECT_NAME}"
)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake"
              "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake"
        DESTINATION lib/cmake/${PROJECT_NAME}
)
install(
  EXPORT ${PROJECT_NAME}-targets
  DESTINATION lib/cmake/${PROJECT_NAME}
  NAMESPACE ${PROJECT_NAME}::
  EXPORT_LINK_INTERFACE_LIBRARIES
)

# CPack
if(${PROJECT_NAME}_TOP_LEVEL)
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
