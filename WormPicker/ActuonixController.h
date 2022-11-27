/*
	ActuonixController.h
	Peter Bowlin
	July 2021

	Class definition for controlling the polulu actuonix linear actuator. This controls the Z action of the picking arm in Wormpicker 2.

*/

#pragma once

// Standard includes
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

#include "HardwareSelector.h"


class ActuonixController {
	public:
		ActuonixController(HardwareSelector& use);

		void homeActuonix();
		bool moveAbsolute(int position, bool blockflag = true, int waittime = 1000);
		bool moveRelative(int position, bool blockflag = true, int waittime = 1000);
		std::string readFromStatus(std::string lookup);
		int readPosition();
		//int getFocusPos() { return focus_position; };

	protected:
		HardwareSelector& use;
		int current_position; // Stores the current location of the linear actuator
		int max_position;
		int min_position;
		//int focus_position;
};