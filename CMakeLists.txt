# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file at the root of the
# source tree or at
# <https://github.com/Krzmbrzl/NamedPipe/blob/main/LICENSE>.

cmake_minimum_required(VERSION 3.15)

project(NamedPipe
	VERSION 1.0.0
	LANGUAGES CXX
)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

include(setup_dependencies)

if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
	set(WANTED_WARNINGS "ENABLE_MOST_WARNINGS")
else()
	set(WANTED_WARNINGS "DISABLE_ALL_WARNINGS")
endif()

include(CompilerFlags)

get_compiler_flags(
	${WANTED_WARNINGS}
	OUTPUT_VARIABLE COMPILER_FLAGS
)

add_compile_options(${COMPILER_FLAGS})

add_subdirectory(src)
add_subdirectory(examples)