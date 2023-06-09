cmake_minimum_required(VERSION 3.2.0)
project(meta)

option(ENABLE_LIBCXX "Use libc++ for the C++ standard library (only for clang)" ON)
option(ENABLE_PROFILING "Link against gperftools profiler library" OFF)
option(ENABLE_JEMALLOC "Link against jemalloc if available" ON)

if (MINGW)
  # jemalloc is breaking things in MSYS2; just disable it for now
  set(ENABLE_JEMALLOC OFF)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

set(MeTA_VERSION_MAJOR 3)
set(MeTA_VERSION_MINOR 0)
set(MeTA_VERSION_PATCH 2)
set(MeTA_VERSION
    "${MeTA_VERSION_MAJOR}.${MeTA_VERSION_MINOR}.${MeTA_VERSION_PATCH}")

if (NOT (MSVC_VERSION AND CMAKE_COMPILER_ID MATCHES "Clang"))
  # MeTA only requires C++11, but on all of our supported compilers at least
  # C++1y support exists, so we therefore set the standard to 14
  set(CMAKE_CXX_STANDARD 14)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)

  if (${CMAKE_VERSION} VERSION_GREATER 3.7)
    # CMake adds support for the C++17 standard in version 3.8. This is
    # needed in newer versions of clang and libc++ to enable std::optional,
    # std::string_view, and friends.
    list(FIND CMAKE_CXX_COMPILE_FEATURES "cxx_std_17" _index)
    if (_index GREATER -1)
      set(CMAKE_CXX_STANDARD 17)
    endif()
  endif()
endif()

set(META_PROJECT_SOURCE_DIR ${PROJECT_SOURCE_DIR})

include(CMakePushCheckState)
include(ExternalProject)
include(deps/meta-cmake/FindOrBuildICU.cmake)
include(deps/meta-cmake/SetClangOptions.cmake)
include(deps/meta-cmake/CompilerKludges.cmake)

find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)
find_package(LibLZMA)

if (LIBLZMA_FOUND AND LIBLZMA_HAS_EASY_ENCODER)
  set(META_HAS_LIBLZMA ON)
endif()

cmake_push_check_state()

# Work around CMake not propagating the standard flag down to the compiler
if (NOT MSVC_VERSION OR ${CMAKE_VERSION} VERSION_GREATER 3.9)
  if (CMAKE_CXX_STANDARD EQUAL 17)
    set(META_CXX_STANDARD_OPTION "${CMAKE_CXX17_STANDARD_COMPILE_OPTION}")
  else()
    set(META_CXX_STANDARD_OPTION "${CMAKE_CXX14_STANDARD_COMPILE_OPTION}")
  endif()
  set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${META_CXX_STANDARD_OPTION}")
endif()

# Check if there is no build type set. If meta itself is the root project,
# compile it in release mode instead. If we aren't the root project, just
# continue along with whatever we would do ordinarily (they *really* should
# be specifying a build type, but...)
if (NOT CMAKE_BUILD_TYPE AND CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  message("-- No build type selected, defaulting to Release")
  set(CMAKE_BUILD_TYPE "Release")
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/deps/findicu)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/deps/meta-cmake/)

FindOrBuildICU(
  VERSION 63.1
  URL http://download.icu-project.org/files/icu4c/63.1/icu4c-63_1-src.tgz
  URL_HASH SHA512=9ab407ed840a00cdda7470dcc4c40299a125ad246ae4d019c4b1ede54781157fd63af015a8228cd95dbc47e4d15a0932b2c657489046a19788e5e8266eac079c
)

add_library(meta-definitions INTERFACE)
target_include_directories(meta-definitions INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

if(UNIX OR MINGW)
  target_compile_options(meta-definitions INTERFACE -Wall -Wextra -pedantic)

  if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    SetClangOptions(meta-definitions)
  endif()
endif()

target_include_directories(meta-definitions SYSTEM INTERFACE ${ZLIB_INCLUDE_DIRS})

if (META_HAS_LIBLZMA)
  target_compile_definitions(meta-definitions INTERFACE -DMETA_HAS_LIBLZMA=1)
endif()

if (LIBDL_LIBRARY)
  target_link_libraries(meta-definitions INTERFACE ${LIBDL_LIBRARY})
endif()

if (CXXABI_LIBRARY)
  target_link_libraries(meta-definitions INTERFACE ${CXXABI_LIBRARY})
endif()

if (LIBCXX_FOUND)
  target_include_directories(meta-definitions SYSTEM INTERFACE ${LIBCXX_INCLUDE_DIR})
  target_compile_options(meta-definitions INTERFACE ${LIBCXX_OPTIONS})
  target_link_libraries(meta-definitions INTERFACE -L${LIBCXX_LIB_PATH})
  target_link_libraries(meta-definitions INTERFACE ${LIBCXX_LIBRARY})
endif()

if (ENABLE_PROFILING)
  find_library(GPERFTOOLS_PROFILER NAMES profiler REQUIRED)
  message("-- Found profiler: ${GPERFTOOLS_PROFILER}")
  target_link_libraries(meta-definitions INTERFACE ${GPERFTOOLS_PROFILER})
endif()

find_library(JEMALLOC_LIB NAMES jemalloc)
if (JEMALLOC_LIB AND ENABLE_JEMALLOC)
  message("-- Using jemalloc: ${JEMALLOC_LIB}")
  target_link_libraries(meta-definitions INTERFACE ${JEMALLOC_LIB})
elseif(NOT MINGW)
  message("-- Using regular malloc; consider installing jemalloc")
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(_DARWIN_USE_64_BIT_INODE 1)
  set(META_IS_DARWIN 1)
  target_compile_definitions(meta-definitions INTERFACE
                             -D_DARWIN_USE_64_BIT_INODE=1)
  target_compile_definitions(meta-definitions INTERFACE
                             -DMETA_IS_DARWIN=1)
endif()

# set a bunch of preprocessor variables to work around various compiler and
# standard library bugs
CompilerKludges(meta/kludges.h)

configure_file(include/meta/config.h.in meta/config.h)
target_include_directories(meta-definitions INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
  $<INSTALL_INTERFACE:include>)

cmake_pop_check_state()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

add_custom_target(tidy
                  COMMAND /usr/bin/rm -rf
                  ./doc
                  *.terms
                  *.phi
                  *.theta)

find_package(Doxygen)
if(DOXYGEN_FOUND AND NOT TARGET doc)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/meta.doxygen.in
                 ${CMAKE_CURRENT_BINARY_DIR}/meta.doxygen @ONLY)
  add_custom_target(doc
                    ${DOXYGEN_EXECUTABLE}
                    ${CMAKE_CURRENT_BINARY_DIR}/meta.doxygen
                    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endif()

add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(deps/cpptoml EXCLUDE_FROM_ALL)

# Warn users that are using a 32-bit system
if (CMAKE_SIZEOF_VOID_P LESS 8)
  message(WARNING "You appear to be running on a 32-bit system. Support \
    for 32-bit systems is provided on a best-effort basis; if at all \
    possible, we strongly recommend that you use  MeTA on a 64-bit \
    platform.")
endif()

# install our targets defined in this file
install(TARGETS meta-definitions
        EXPORT meta-exports
        DESTINATION lib)

# install all of our includes
install(DIRECTORY include/meta/
        DESTINATION include/meta
        FILES_MATCHING PATTERN "*.h"
                       PATTERN "*.tcc")
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/meta/
        DESTINATION include/meta
        FILES_MATCHING PATTERN "*.h")

# generate our ConfigVersion.cmake file
configure_file(cmake/MeTAConfigVersion.cmake.in
               ${CMAKE_CURRENT_BINARY_DIR}/MeTA/MeTAConfigVersion.cmake
               @ONLY)
configure_file(cmake/MeTAConfig.cmake.in
               ${CMAKE_CURRENT_BINARY_DIR}/MeTA/MeTAConfig.cmake
               COPYONLY)

# install our exports and cmake files
install(EXPORT meta-exports
        FILE MeTATargets.cmake
        DESTINATION lib/cmake/MeTA)
install(FILES
          ${CMAKE_CURRENT_BINARY_DIR}/MeTA/MeTAConfigVersion.cmake
          ${CMAKE_CURRENT_BINARY_DIR}/MeTA/MeTAConfig.cmake
        DESTINATION
          lib/cmake/MeTA)

# allow consumption of a build directory as an "installed" version
export(EXPORT meta-exports
       FILE ${CMAKE_CURRENT_BINARY_DIR}/MeTA/MeTATargets.cmake)
export(PACKAGE MeTA)
