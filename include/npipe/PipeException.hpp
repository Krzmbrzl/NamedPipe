// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// source tree or at
// <https://github.com/Krzmbrzl/NamedPipe/blob/main/LICENSE>.

#pragma once

#include "npipe/Exception.hpp"

#include <string>

namespace npipe {

/**
 * An exception thrown by NamedPipe operations if something didn't go as intended
 *
 * @tparam error_code_t The type of the underlying error code
 */
template< typename error_code_t > class PipeException : public Exception {
private:
	/**
	 * The error code associated with the encountered error
	 */
	error_code_t m_errorCode;
	/**
	 * A message for displaying purpose that explains what happened
	 */
	std::string m_message;

public:
	/**
	 * @param errorCode The encountered error code
	 * @param context The context in which this error code has been encountered. This will be embedded in this
	 * exception's message
	 */
	PipeException(error_code_t errorCode, const std::string &context) : m_errorCode(errorCode) {
		m_message = std::string("Pipe action \"") + context + "\" returned error code " + std::to_string(m_errorCode);
	}

	const char *what() const noexcept { return m_message.c_str(); }
};

} // namespace pipe
