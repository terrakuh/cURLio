find_package(Threads REQUIRED)

file(GLOB examples "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
foreach(example ${examples})
  get_filename_component(name "${example}" NAME_WE)

  add_executable(${name} "${example}")
  target_link_libraries(${name} PRIVATE curlio::curlio Threads::Threads)
  set_target_properties(${name} PROPERTIES CXX_STANDARD 20)

  # target_compile_options(${name} PUBLIC -fsanitize=address -fno-omit-frame-pointer -O1
  # -fno-optimize-sibling-calls) target_link_options(${name} PUBLIC -fsanitize=address)
endforeach()
