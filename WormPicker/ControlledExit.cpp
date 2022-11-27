/*
	ControlledExit.cpp
	Anthony Fouad
	Fall 2018
	If the statement is false, perform a controlled exit.
	Usage is similar to assert(test)
*/

#include "ControlledExit.h"
#include <string>
#include <iostream>

void controlledExit(bool test, std::string msg) {

	if (!test) {
		std::cout << std::endl << msg << std::endl;
		std::cout << "Press ENTER to abort program...";
		std::cin.get();
		std::exit(0);
	}

}