// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// source tree or at
// <https://github.com/Krzmbrzl/NamedPipe/blob/main/LICENSE>.

#include <npipe/Exception.hpp>
#include <npipe/InterruptException.hpp>
#include <npipe/NamedPipe.hpp>
#include <npipe/TimeoutException.hpp>

#include <chrono>
#include <iostream>
#include <string>

int main() {
	try {
		while (true) {
			std::string message;
			std::getline(std::cin, message);

			std::cout << "Writing message '" << message << "'" << std::endl;

			try {
				npipe::NamedPipe::write("testPipe", message, std::chrono::seconds(1));
			} catch (const npipe::TimeoutException &) {
				std::cout << "Couldn't deliver message within one second -> dismissing" << std::endl;
			}
		}
	} catch (const npipe::InterruptException &) {
		std::cout << "Write pipe got interrupted" << std::endl;
	} catch (const npipe::Exception &e) {
		std::cout << "[ERROR]: " << e.what() << std::endl;
	}
}
