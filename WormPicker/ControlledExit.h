/*
	ControlledExit.h
	Anthony Fouad
	Fall 2018
	If the statement is false, perform a controlled exit.
	Usage is similar to assert(test)
*/

#pragma once
#ifndef CONTROLLEDEXIT_H
#define CONTROLLEDEXIT_H

#include <string>

void controlledExit(bool test, std::string msg = "Test assertion failed!");

#endif