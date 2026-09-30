#pragma once
#include <exception>
#include <cstdint>

namespace aurora::system {
enum class Operation { Restart, ReturnToMenu, Shutdown, Reboot };

class Request final : public std::exception {
public:
	Request(Operation operation, uint32_t code) : operation(operation), code(code) {}
	const char* what() const noexcept override { return "Application reset requested"; }
	Operation operation;
	uint32_t code;
};

void set_reset_button(bool pressed);
void press_power_button();
}
