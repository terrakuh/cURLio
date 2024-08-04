find_package(Threads REQUIRED)

file(GLOB examples "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
foreach(example ${examples})
  get_filename_component(name "${example}" NAME_WE)

  add_executable(curlio_example_${name} "${example}")
  target_link_libraries(curlio_example_${name} PRIVATE cURLio::cURLio Threads::Threads)
  set_target_properties(curlio_example_${name} PROPERTIES CXX_STANDARD 20)

  # target_compile_options(curlio_example_${name} PUBLIC -fsanitize=address -fno-omit-frame-pointer -O1
  # -fno-optimize-sibling-calls) target_link_options(curlio_example_${name} PUBLIC -fsanitize=address)
endforeach()
