// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// source tree or at
// <https://github.com/Krzmbrzl/NamedPipe/blob/main/LICENSE>.

#pragma once

#include "npipe/Exception.hpp"

namespace npipe {
/**
 * An exception thrown when an operation takes longer than allowed
 */
class TimeoutException : public Exception {
public:
	const char *what() const noexcept { return "TimeoutException"; }
};

} // namespace npipe
