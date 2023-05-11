# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file at the root of the
# source tree or at
# <https://github.com/Krzmbrzl/NamedPipe/blob/main/LICENSE>.

cmake_minimum_required(VERSION 3.15)

project(NamedPipe
	VERSION 1.0.0
	LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

add_subdirectory(src)
add_subdirectory(examples)
