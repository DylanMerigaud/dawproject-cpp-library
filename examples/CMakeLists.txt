# Each example runs with no arguments, works in a temporary directory and exits 0.
set(DAWPROJECT_EXAMPLES write_project read_project inspect_archive)
foreach(example IN LISTS DAWPROJECT_EXAMPLES)
  add_executable(${example} ${example}.cpp)
  target_link_libraries(${example} PRIVATE dawproject::dawproject)
  target_compile_options(${example} PRIVATE ${DAWPROJECT_WARNING_FLAGS})
  add_test(NAME example_${example} COMMAND ${example})
endforeach()
