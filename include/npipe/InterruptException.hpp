// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// source tree or at
// <https://github.com/Krzmbrzl/NamedPipe/blob/main/LICENSE>.

#pragma once

#include "npipe/Exception.hpp"

namespace npipe {

/**
 * Exception thrown when a pipe operation is interrupted
 */
struct InterruptException : public Exception {};

} // namespace npipe
