/*
	DynamixelServo.cpp
	Anthony Fouad
	April 2018
*/

#include "stdafx.h"
#include "ControlledExit.h"
#include "DynamixelServo.h"
#include "ThreadFuncs.h"
#include "AnthonysTimer.h" // Included in cpp file intentionally to avoid redefinitions.
#include <cmath>

using namespace std;

// Constructor
DynamixelServo::DynamixelServo(int this_id, int ma, int Ma, int exitpos) :
	  id(this_id), 
	  min_angle(ma), 
	  max_angle(Ma),
	  exit_pos(exitpos)
{
	
	// Set goal position to be in the middle
	goal_pos = Ma;

	// Verify that the min and max angles are valid
	if (min_angle > max_angle) {
		cout << "ERROR: min angle " << min_angle << " on Servo #" << id << " is not less than max angle" << max_angle << "!\n";
		assert(0);
	}

	if (max_angle > 10000 || max_angle < 0) {
		cout << "ERROR: max angle " << max_angle << " on Servo #" << id << "is invalid!!\n";
		assert(0);
	}

	if (min_angle > 10000 || min_angle < 0) {
		cout << "ERROR: min angle " << min_angle << " on Servo #" << id << "is invalid!!\n";
		assert(0);
	}

	// Return now without sending commands to servo if we are using OFFLINE_MODE
		// if (OFFLINE_MODE) { return; }
		// in offline mode this object will not exist

	// Turn torque off prior to modifying homing offset
	dxl2_write_byte(id, DXL_TORQUE_ENABLE, 0);
	if (dxl_get_comm_result() != COMM_RXSUCCESS) { printf("DXL %d : Failed to disable torque during startup\n", id); }

	// Set the homing offset of the servo - disabled. Use R+ Manager for now.
	//dxl2_write_dword(id, DXL_HOMING_OFFSET, ho);
	//if (dxl_get_comm_result() != COMM_RXSUCCESS) { printf("DXL %d : Failed to write homing offset\n", id); }

	// Read current position to make sure it is in-bounds
	present_pos = (long)dxl2_read_dword(id, DXL_PRESENT_POSITION);
	controlledExit(present_pos > min_angle && present_pos < max_angle, 
						"ERROR on DXL " + to_string(id) + ": Initially out of bounds at pos: " + 
						to_string(present_pos) + "\n");

	// Turn on torque for servo;
	dxl2_write_byte(id, DXL_TORQUE_ENABLE, 1);
	if (dxl_get_comm_result() != COMM_RXSUCCESS) { printf("DXL %d : Failed to enable torque during startup\n", id); }
}

// Destructor
DynamixelServo::~DynamixelServo() {

	// Move to the maximum and WAIT for movement to complete
	goToAbsolute(exit_pos,true);
	boost_sleep_ms(300);

	// Turn off torque for servo (in offline mode, this object will not exist)
	dxl2_write_byte(id, DXL_TORQUE_ENABLE, 0);

}

// Move to position - block until complete or 1s passes if blockflag == true. Return false if out of bounds. 
bool DynamixelServo::goToAbsolute(int target, bool blockflag, bool suppressOOBwarn) {

	// Lock access to all servo objects (static mutex) until current thread is finished using it
	boost::lock_guard<boost::mutex> lock{ Mutex }; 
		/// This is INTERNAL locking
		/// https://www.boost.org/doc/libs/1_58_0/doc/html/thread/synchronization.html#thread.synchronization.mutex_concepts
		/// It won't protect against simultanous requests from outside the class (EXTERNAL)

	// Verify that position is in-bounds, if not, set it to remain unchanged and mark unsucessful
	if (target < min_angle || target > max_angle) {
		if (!suppressOOBwarn)
			cout << "WARNING: Position " << target << " on Servo #" << id << " is out of bounds!\n";
		target = goal_pos;
		return false;
	}
	else {
		goal_pos = target;
	}

	// In offline mode, DynamixelServo objects should never be created by the controller object
		// if (OFFLINE_MODE) { return true; }

	// Force torque to be on
	dxl2_write_byte(id, DXL_TORQUE_ENABLE, 1);
	if (dxl_get_comm_result() != COMM_RXSUCCESS) { printf("DXL %d : Failed to enable torque during movement\n", id); }

	// Move to goal position. If write failed try again a couple times
	if (!dxl2_write_dword_robust(id, DXL_GOAL_POSITION, target)){ 
		cout << "dxl2 write failed" << endl;
		return false; 
	}

	// Read position to tell when movement is completed, but only 1 second allowed
	bool is_moving = false;
	int resolution = 10, load_limit = 50;
	AnthonysTimer Ta;

	while ( Ta.getElapsedTime() < 1 && blockflag)
	{
		// Measure actual real time position
		present_pos = dxl2_read_dword_robust(id, DXL_PRESENT_POS);
		present_load = dxl2_read_dword_robust(id, DXL_PRESENT_LOAD);

		if (present_pos == -999999 || present_load == -999999)
			return false;

		if (dxl_get_comm_result() != COMM_RXSUCCESS)
			printf(" ID %d : Read Failed\n", id);
		else

		// Exit if reached desired position
		if (abs(present_pos - target) < resolution) { break; } 
	}

	//cout << "Dxl goToAbsolute success!" << endl;
	return true;
}

// Go to relative position
bool DynamixelServo::goToRelative(int dpos, bool blockflag, bool suppressOOBwarn ) {

	return goToAbsolute(goal_pos + dpos, blockflag, suppressOOBwarn);

}

// Go to maximum position
bool DynamixelServo::goToMaximum(bool blockflag) {
	return goToAbsolute(max_angle-4, blockflag);
}

// Go to minimum position
bool DynamixelServo::goToMinimum(bool blockflag) {
	return goToAbsolute(min_angle + 4, blockflag);
}

// Getters
int DynamixelServo::getId() const { return id; }
int DynamixelServo::getPosition() const { return goal_pos; }	/// present_pos is only accurate if blocking is in use.

// Low level wrappers for dxl2 functions
bool DynamixelServo::dxl2_write_dword_robust(int id, int address, int value) {

	int iter = 0;
	do {
		dxl2_write_dword(id, address, value); iter++;
	} while (dxl_get_comm_result() != COMM_RXSUCCESS && iter < 5);

	if (dxl_get_comm_result() != COMM_RXSUCCESS) { printf("Dynamixel ID %d : Write dword failed even after 5 tries\n", id); return false; }

	return true;
}

int DynamixelServo::dxl2_read_dword_robust(int id, int address) {

	int value = -999999, iter= 0;
	do {
		value = (int)dxl2_read_dword(id, address); iter++;
	} while (dxl_get_comm_result() != COMM_RXSUCCESS && iter < 5);

	if (dxl_get_comm_result() != COMM_RXSUCCESS) { printf("Dynamixel ID %d : Read dword failed even after 5 tries\n", id);  }

	return value;
}

// Static variables
boost::mutex DynamixelServo::Mutex;
