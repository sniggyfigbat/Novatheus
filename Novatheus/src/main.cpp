#include "pch.h"
#include "utils/utils.h"
#include "core\central.h"
#include <float.h>

int main(int argc, char* argv[]) {
	INFO("Program initialising...");

#ifdef DEBUG
	CRITICAL("Warning: Program running in DEBUG mode! This is approximately two orders of magniture slower than RELEASE mode! Switching to RELEASE is highly recommended!");
#endif

	Core::CentralController central;

	INFO("Program initialised.");

	while (central.runLoop()) {}

	INFO("Program terminating...");
	INFO("Program terminated. Press enter to close the console.");
	std::cin.get();
	return 0;
}