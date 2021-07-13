/*
    This file is part of wavedcc,
    Copyright (C) 2021 Glenn Butcher.
    
    wavedcc is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    
    wavedcc is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License
    along with wavedcc.  If not, see <http://www.gnu.org/licenses/>.
*/

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

