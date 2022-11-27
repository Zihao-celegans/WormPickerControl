/*
	DynamixelController.h
	Anthony Fouad
	April 2018

	Class definition for accessing the USB2Dynamixel. 

	Each servo is represented by an object of type DynamixelServo. These objects contain information about the 
	position limits for those servos.

	Servos objects are referred to by their actual ID, e.g. "1", and not by the array index starting at 0.

	Servos objects within the vector are allocated from heap memory. If allocated from stack, they appear to go out of
	scope and destruct when the constructor finishes.

	You should use R+ manager to set the ID's of the servos. Note that Torque Enable (64) must be cleared to change ID.

	You should use R+ manager to set any homing offsets. For unknown reasons, modifying the homing offset through the 
	SDK leads to unpredictable positioning of the motor. For that reason, control of the homing offsets has been disabled
	even if homing offsets are imported and stored in the servo objects

	DynamixelController.h should #include DynamixelServo.h AFTER defining everything.


*/
#pragma once
#ifndef DYNAMIXELCONTROLLER_H
#define DYNAMIXELCONTROLLER_H

// Standard includes
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

// DYNAMIXEL includes and control table addresses 
#include "DynamixelControlTable.h"
#include "DynamixelServo.h"
#include "HardwareSelector.h"

//using namespace std; // namespaces should not be used in header files because then when you import headers you add namespaces without realizing it.

class DynamixelController {

public:

	// Construct object and initialize controller
	DynamixelController(HardwareSelector& _use);						/// Blank controller object
	// DynamixelController(string fDynParams, HardwareSelector& _use);		/// Initialized controller object

	// Close USB2dynamixel and delete objects
	~DynamixelController();

	// Move servo with ID "id" to position "pos"
	bool moveServo(int id, int pos, bool rel = DXL_ABSOLUTE, bool blockflag = false, bool suppressOOBwarn = false);

	// Move servo with ID "id" to its maximum or minimum position
	bool resetServo(int id);
	bool resetServoToMin(int id);

	// Move servos manually by key press
	/// Key test mode checks whether the keynumber matches any command but does not actually execute the command
	bool moveServoByKey(int &keynum, bool keytest = false);

	// Getter
	int numberOfServos();				/// How many servos in this controller
	int positionOfServo(int id);		/// Position of servo "id". If "id" doesn't exist, return -1



protected:

	std::vector <DynamixelServo*> Servos;	/// List of all available servos and their limits
	int port_number, baud_rate;			/// Parameters of the serial port, generally COM8, 57600
	int man_int;						/// Amount to move servos by during manual movement.
	HardwareSelector &use;				/// Whether to use DXL hardware
};

#endif