cmake_minimum_required(VERSION 3.16...3.31)

# miniz is a C library, so C is enabled next to C++.
project(dawproject VERSION 0.1.0 DESCRIPTION "Read and write DAWproject files" LANGUAGES C CXX)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  set(DAWPROJECT_IS_TOP_LEVEL ON)
else()
  set(DAWPROJECT_IS_TOP_LEVEL OFF)
endif()

option(DAWPROJECT_BUILD_TESTS "Build the dawproject tests" ${DAWPROJECT_IS_TOP_LEVEL})
option(DAWPROJECT_BUILD_EXAMPLES "Build the dawproject examples" ${DAWPROJECT_IS_TOP_LEVEL})
option(DAWPROJECT_INSTALL "Generate the dawproject install rules" ${DAWPROJECT_IS_TOP_LEVEL})
option(DAWPROJECT_USE_SYSTEM_PUGIXML "Use a pugixml found with find_package instead of the vendored copy" OFF)
option(DAWPROJECT_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

include(GNUInstallDirs)

set(DAWPROJECT_SOURCES
  src/archive.cpp
  src/enums.cpp
  src/error.cpp
  src/model.cpp
  src/numbers.cpp
  src/validate.cpp
  src/xml_read.cpp
  src/xml_write.cpp
)

# miniz is compiled through src/miniz_impl.c, which renames its public functions first.
add_library(dawproject ${DAWPROJECT_SOURCES} src/miniz_impl.c)
add_library(dawproject::dawproject ALIAS dawproject)

# C++17 at least; a host that sets a newer CMAKE_CXX_STANDARD builds the library with it.
target_compile_features(dawproject PUBLIC cxx_std_17)
# Only the functions and classes marked DAWPROJECT_EXPORT leave a shared build.
set_target_properties(dawproject PROPERTIES
  CXX_EXTENSIONS OFF
  C_VISIBILITY_PRESET hidden
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
  EXPORT_NAME dawproject
)

include(GenerateExportHeader)
set(DAWPROJECT_GENERATED_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/include)
generate_export_header(dawproject
  BASE_NAME DAWPROJECT
  EXPORT_FILE_NAME ${DAWPROJECT_GENERATED_INCLUDE_DIR}/dawproject/export.hpp
)
get_target_property(DAWPROJECT_TYPE dawproject TYPE)
if(NOT DAWPROJECT_TYPE STREQUAL "SHARED_LIBRARY")
  target_compile_definitions(dawproject PUBLIC DAWPROJECT_STATIC_DEFINE)
endif()

target_include_directories(dawproject
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${DAWPROJECT_GENERATED_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniz
)

# miniz: no stdio (files go through std::fstream callbacks) and no zlib compatible names that
# could clash with a zlib in the same program; its own names carry a dawproject_ prefix
# (src/miniz_rename.h).
target_compile_definitions(dawproject PRIVATE
  MINIZ_NO_ZLIB_COMPATIBLE_NAMES
  MINIZ_NO_STDIO
)

if(DAWPROJECT_USE_SYSTEM_PUGIXML)
  find_package(pugixml CONFIG REQUIRED)
  target_link_libraries(dawproject PRIVATE pugixml::pugixml)
else()
  target_sources(dawproject PRIVATE third_party/pugixml/pugixml.cpp)
  target_include_directories(dawproject PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pugixml)
  # The vendored pugixml lives in a private namespace so it never clashes with a pugixml the
  # consuming program links on its own.
  target_compile_definitions(dawproject PRIVATE pugi=dawproject_pugi)
endif()

# Warnings apply to this library's own sources, not to the vendored ones.
if(MSVC)
  set(DAWPROJECT_WARNING_FLAGS /W4 /permissive-)
  if(DAWPROJECT_WARNINGS_AS_ERRORS)
    list(APPEND DAWPROJECT_WARNING_FLAGS /WX)
  endif()
else()
  set(DAWPROJECT_WARNING_FLAGS -Wall -Wextra -Wpedantic -Wshadow)
  if(DAWPROJECT_WARNINGS_AS_ERRORS)
    list(APPEND DAWPROJECT_WARNING_FLAGS -Werror)
  endif()
endif()
set_source_files_properties(${DAWPROJECT_SOURCES} PROPERTIES COMPILE_OPTIONS "${DAWPROJECT_WARNING_FLAGS}")
if(MSVC)
  target_compile_options(dawproject PRIVATE /utf-8)
  target_compile_definitions(dawproject PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if(DAWPROJECT_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(DAWPROJECT_BUILD_EXAMPLES)
  enable_testing()
  add_subdirectory(examples)
endif()

if(DAWPROJECT_INSTALL)
  include(CMakePackageConfigHelpers)
  set(DAWPROJECT_CMAKE_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/dawproject)

  install(TARGETS dawproject
    EXPORT dawprojectTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )
  install(DIRECTORY include/dawproject DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
  install(FILES ${DAWPROJECT_GENERATED_INCLUDE_DIR}/dawproject/export.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/dawproject)
  install(EXPORT dawprojectTargets
    NAMESPACE dawproject::
    DESTINATION ${DAWPROJECT_CMAKE_DIR}
  )
  configure_package_config_file(
    cmake/dawprojectConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/dawprojectConfig.cmake
    INSTALL_DESTINATION ${DAWPROJECT_CMAKE_DIR}
  )
  write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/dawprojectConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion
  )
  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/dawprojectConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/dawprojectConfigVersion.cmake
    DESTINATION ${DAWPROJECT_CMAKE_DIR}
  )
  set(DAWPROJECT_LICENSE_DIR ${CMAKE_INSTALL_DATADIR}/licenses/dawproject)
  install(FILES LICENSE DESTINATION ${DAWPROJECT_LICENSE_DIR})
  install(FILES third_party/pugixml/LICENSE.md
    DESTINATION ${DAWPROJECT_LICENSE_DIR} RENAME LICENSE.pugixml.md)
  install(FILES third_party/miniz/LICENSE
    DESTINATION ${DAWPROJECT_LICENSE_DIR} RENAME LICENSE.miniz)
endif()
