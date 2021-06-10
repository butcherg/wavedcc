

#include <iostream>
#include <string>

#include "dccengine.h"

int main (int argc, char **argv)
{
	std::string initresult = dccInit();
	std::string cmd;
	
	while (true) {
		std::cout << "> " << std::flush;
		std::getline(std::cin, cmd);
		if (cmd == "exit") break;
		
		std::string response = dccCommand(cmd); 
		std::cout << response << std::endl;
		
	}

	std::cout << "wavedcc exiting..." << std::endl;

	dccFinish();
	exit(0);
}

