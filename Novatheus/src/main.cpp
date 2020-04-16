#include "pch.h"
#include "utils/utils.h"

int main(int argc, char* argv[]) {
	INFO("Program initialising...");
	INFO("Program initialised.");

	INFO("Program terminating...");
	INFO("Program terminated. Press any key to continue...");
	std::cin.get();
	return 0;
}