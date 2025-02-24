# 
# MIT License
# 
# Copyright (c) 2025 The Dyne Language Team
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
# 

cmake_minimum_required(
  VERSION 3.15.0 
  FATAL_ERROR
)

project( 
    dynelang
    VERSION 0.1.0
    LANGUAGES CXX
)

# Fix behavior of CMAKE_CXX_STANDARD when targeting macOS.
if (POLICY CMP0025)
    cmake_policy(SET CMP0025 NEW)
endif ()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(GTest REQUIRED)
# brew install googletest

set(dynec_srcs)
set(dynec_hdrs)
set(dynec_cmake CMakeLists.txt)


include(src/CMakeLists.txt)


# option(MYLIB_BUILD_TESTS "Build mylib tests" OFF)
# if(DEFINED MYLIB_SHARED_LIBS) ...

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}
  FILES
  ${dynec_srcs}
  ${dynec_hdrs}
  ${dynec_cmake}
  src/dynec.cpp
  test/test.cpp
)

add_executable(
  # executable name
  dynec
  # source files
  src/dynec.cpp
  ${dynec_srcs}
  ${dynec_hdrs}
  # build files
  ${dynec_cmake}
  # other files for quick access
  README.md
  ## Package.md
)

set_source_files_properties(README.md PROPERTIES HEADER_FILE_ONLY TRUE)
# set_source_files_properties(Package.md PROPERTIES HEADER_FILE_ONLY TRUE)

#target_include_directories(<target> [SYSTEM] [AFTER|BEFORE]
#  <INTERFACE|PUBLIC|PRIVATE> [items1...]


target_include_directories(
  dynec
  PUBLIC
  include
)

if(MSVC)
  target_compile_options(dynec PRIVATE /W4 /WX)
else()
  target_compile_options(dynec PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()


add_executable(
  # executable name
  dynetest
  # source files
  test/test.cpp
  ${dynec_srcs}
  ${dynec_hdrs}
  # build files
  ${dynec_cmake}
)

target_link_libraries(dynetest
  PRIVATE
    GTest::GTest
    GTest::Main
)

target_include_directories(
  dynetest
  PUBLIC
  include
)

if(MSVC)
  target_compile_options(dynetest PRIVATE /W4 /WX)
else()
  target_compile_options(dynetest PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
