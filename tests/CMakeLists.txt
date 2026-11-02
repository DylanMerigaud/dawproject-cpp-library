set(DAWPROJECT_XSD_OUT_DIR ${CMAKE_BINARY_DIR}/xsd-out)

add_executable(dawproject_tests
  test_main.cpp
  test_archive.cpp
  test_enums.cpp
  test_fixtures.cpp
  test_model.cpp
  test_numbers.cpp
  test_scenarios.cpp
  test_validate.cpp
  test_xml.cpp
)
target_link_libraries(dawproject_tests PRIVATE dawproject::dawproject)
target_include_directories(dawproject_tests PRIVATE ${PROJECT_SOURCE_DIR}/third_party/doctest)
target_compile_definitions(dawproject_tests PRIVATE
  # Include the standard headers instead of doctest's own forward declarations of std types,
  # which recent MSVC versions reject (warning C5285).
  DOCTEST_CONFIG_USE_STD_HEADERS
  DAWPROJECT_SPEC_DIR="${PROJECT_SOURCE_DIR}/spec"
  DAWPROJECT_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
  DAWPROJECT_XSD_OUT_DIR="${DAWPROJECT_XSD_OUT_DIR}"
  DAWPROJECT_TEST_PROJECT_VERSION="${PROJECT_VERSION}"
)
target_compile_options(dawproject_tests PRIVATE ${DAWPROJECT_WARNING_FLAGS})
if(MSVC)
  target_compile_options(dawproject_tests PRIVATE /utf-8 /bigobj)
endif()

# Start every run from an empty schema check directory.
add_test(NAME xsd_out_clean
  COMMAND ${CMAKE_COMMAND} -E remove_directory ${DAWPROJECT_XSD_OUT_DIR})
set_tests_properties(xsd_out_clean PROPERTIES FIXTURES_SETUP xsd_clean)

set(DAWPROJECT_TEST_SUITES archive enums fixtures model numbers scenarios validate version xml)
foreach(suite IN LISTS DAWPROJECT_TEST_SUITES)
  add_test(NAME unit.${suite} COMMAND dawproject_tests --test-suite=${suite})
  set_tests_properties(unit.${suite} PROPERTIES
    FIXTURES_REQUIRED xsd_clean
    FIXTURES_SETUP xsd_dump)
endforeach()

# Every XML the unit tests wrote is validated against the official XSD files.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  add_test(NAME xsd_validate
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/xsd_validate.py
            ${DAWPROJECT_XSD_OUT_DIR} ${PROJECT_SOURCE_DIR}/spec)
  set_tests_properties(xsd_validate PROPERTIES
    FIXTURES_REQUIRED "xsd_clean;xsd_dump"
    SKIP_RETURN_CODE 77)
else()
  message(STATUS "dawproject: Python 3 not found, the XSD validation test is not registered")
endif()

# Every ```cpp block of README.md is a complete program: each one is built against the library
# and run in its own empty directory, so the README cannot drift from the API.
set(DAWPROJECT_README ${PROJECT_SOURCE_DIR}/README.md)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${DAWPROJECT_README})
file(READ ${DAWPROJECT_README} DAWPROJECT_README_TEXT)
set(DAWPROJECT_README_FENCE "```cpp\n")
string(LENGTH "${DAWPROJECT_README_FENCE}" DAWPROJECT_README_FENCE_LENGTH)
set(DAWPROJECT_README_BLOCK 0)
while(TRUE)
  string(FIND "${DAWPROJECT_README_TEXT}" "${DAWPROJECT_README_FENCE}" start)
  if(start EQUAL -1)
    break()
  endif()
  math(EXPR start "${start} + ${DAWPROJECT_README_FENCE_LENGTH}")
  string(SUBSTRING "${DAWPROJECT_README_TEXT}" ${start} -1 DAWPROJECT_README_TEXT)
  string(FIND "${DAWPROJECT_README_TEXT}" "```" end)
  if(end EQUAL -1)
    message(FATAL_ERROR "README.md: unterminated cpp block")
  endif()
  string(SUBSTRING "${DAWPROJECT_README_TEXT}" 0 ${end} code)
  math(EXPR end "${end} + 3")
  string(SUBSTRING "${DAWPROJECT_README_TEXT}" ${end} -1 DAWPROJECT_README_TEXT)

  math(EXPR DAWPROJECT_README_BLOCK "${DAWPROJECT_README_BLOCK} + 1")
  set(name readme_block_${DAWPROJECT_README_BLOCK})
  set(source ${CMAKE_CURRENT_BINARY_DIR}/readme/${name}.cpp)
  # Written through configure_file so an unchanged block is not rebuilt.
  file(WRITE ${source}.in "${code}")
  configure_file(${source}.in ${source} COPYONLY)
  add_executable(${name} ${source})
  target_link_libraries(${name} PRIVATE dawproject::dawproject)
  target_compile_options(${name} PRIVATE ${DAWPROJECT_WARNING_FLAGS})
  set(workdir ${CMAKE_CURRENT_BINARY_DIR}/readme/${name}-run)
  add_test(NAME readme.block${DAWPROJECT_README_BLOCK}.clean
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${workdir})
  add_test(NAME readme.block${DAWPROJECT_README_BLOCK}.workdir
    COMMAND ${CMAKE_COMMAND} -E make_directory ${workdir})
  add_test(NAME readme.block${DAWPROJECT_README_BLOCK} COMMAND ${name} WORKING_DIRECTORY ${workdir})
  set_tests_properties(readme.block${DAWPROJECT_README_BLOCK}.clean PROPERTIES
    FIXTURES_SETUP readme_${DAWPROJECT_README_BLOCK}_clean)
  set_tests_properties(readme.block${DAWPROJECT_README_BLOCK}.workdir PROPERTIES
    FIXTURES_REQUIRED readme_${DAWPROJECT_README_BLOCK}_clean
    FIXTURES_SETUP readme_${DAWPROJECT_README_BLOCK})
  set_tests_properties(readme.block${DAWPROJECT_README_BLOCK} PROPERTIES
    FIXTURES_REQUIRED readme_${DAWPROJECT_README_BLOCK})
endwhile()
if(DAWPROJECT_README_BLOCK EQUAL 0)
  message(FATAL_ERROR "README.md has no cpp block")
endif()
