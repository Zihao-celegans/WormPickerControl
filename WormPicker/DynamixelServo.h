/*
	DynamixelServo.h
	Anthony Fouad
	April 2018

	Class containing information about indvidual servo motors, especially 
	the acceptable range for each and their homing offset

	DynamixelController.h should #include DynamixelServo.h AFTER defining everything.
*/
#pragma once
#ifndef DYNAMIXELSERVO_H
#define DYNAMIXELSERVO_H

#include "DynamixelControlTable.h"
#include <iostream>
#include "boost\assert.hpp"
#include "boost\thread.hpp"
//using namespace std;

class DynamixelServo {

public:

	// Constructor
	DynamixelServo(int this_id, int ma, int Ma, int exitpos);

	// Destructor
	~DynamixelServo();

	// Move to position - block until complete or 1s passes if requested
	bool goToAbsolute(int pos, bool blockflag = false, bool suppressOOBwarn = false);		
	bool goToRelative(int dpos, bool blockflag = false, bool suppressOOBwarn = false);
	bool goToMaximum(bool blockflag = true);
	bool goToMinimum(bool blockflag = true);

	// Getters
	int getId() const;
	int getPosition() const;

	// Low level wrappers for DXL functions to increase robustness
	bool dxl2_write_dword_robust(int id, int address, int value);
	int dxl2_read_dword_robust(int id, int address);

	// Static variables
	static boost::mutex Mutex;		/// Static mutex to be shared among all servo objects everywhere


protected:

	int id;							/// ID of the servo (set using R+ manager, can't have two of the same)
	int min_angle, max_angle;		/// Acceptable range of motion for this servo
	int goal_pos;					/// Current goal position
	int present_pos;				/// Current position of the servo (measured, not commanded)
	int	exit_pos;					/// Position to go to when powering down system
	int present_load;				/// Current load (torque) on the servo. Can be pos or neg depending on direction. 
	int homing_offset;
};

#endif