# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file at the root of the
# source tree or at
# <https://github.com/Krzmbrzl/NamedPipe/blob/main/LICENSE>.

add_executable(reader Reader.cpp)
add_executable(writer Writer.cpp)

target_link_libraries(reader PRIVATE NamedPipe::NamedPipe)
target_link_libraries(writer PRIVATE NamedPipe::NamedPipe)
