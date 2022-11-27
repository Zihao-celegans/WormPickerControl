/*
	DynamixelController.cpp
	Anthony Fouad
	April 2018
*/

#include "stdafx.h"
#include "DynamixelController.h"
#include "AnthonysKeyCodes.h"
#include "ControlledExit.h"
#include "Config.h"
#include "json.hpp"
#include <fstream>

using json = nlohmann::json;
using namespace std;

// Construct blank object
/*
DynamixelController::DynamixelController(HardwareSelector& _use) : use(_use) {
	man_int = 25;
	port_number = 0;
	baud_rate = 0;
}*/

// Construct object and initialize controller
DynamixelController::DynamixelController(HardwareSelector &_use) : use(_use) {

	// Load the file from disk
	cout << "Initializing DYNAMIXEL servos...\n" << "\tReading port info from disk...\n";
	json config = Config::getInstance().getConfigTree("dynamixel");

	port_number = config["port"].get<int>();
	baud_rate = config["baud rate"].get<int>();
	cout << "\tport: " << port_number << ", " << baud_rate << " bps\n";

	// Connect to the DYNAMIXEL
	if (use.dxl) {
		if (dxl_initialize(port_number, baud_rate) == 0) 
			controlledExit(0, "ERROR: Failed to communicate with dynamixel controller\n");
	}

	if (use.dxl) {

		// load each dxl
		for (json servo : config["servos"]) {
			
			// read each param
			int this_id, ma, Ma, exitpos;
			this_id = servo["id"].get<int>();
			ma = servo["min angle"].get<int>();
			Ma = servo["max angle"].get<int>();
			exitpos = servo["exit pos"].get<int>();
			
			// create the servo
			Servos.push_back(new DynamixelServo(this_id, ma, Ma, exitpos));						/// each serv object stored on heap: https://www.quora.com/C++-programming-language-Does-std-vector-use-stack-or-heap-memory-and-why
			cout << "\t[" << "\t" << this_id << "\t" << ma << "\t" << Ma << "\t" << exitpos << "]" << endl;
		}
	}
	
	man_int = 25;

	return;
}

// Destructor - delete all servo objects by clearing the vector
DynamixelController::~DynamixelController() {

	// Delete the heap servo objects and move to 
	for (int i = 0; i < Servos.size(); ++i) {
		delete Servos[i];
	}

	// Terminate DXL interface
	if (use.dxl) { dxl_terminate(); }

	// Clear the vector of pointers itself
	Servos.clear();
}

// Move servo with ID id to position or relative position. If no servo with this ID, nothing moves. Default to absolute movement
bool DynamixelController::moveServo(int id, int pos, bool rel, bool blockflag, bool suppressOOBwarn) {

	for (int i = 0; i < Servos.size(); i++) {
		if			(Servos[i]->getId() == id && !rel) { return Servos[i]->goToAbsolute(pos,blockflag, suppressOOBwarn); }
		else if		(Servos[i]->getId() == id)		  { return Servos[i]->goToRelative(pos, blockflag, suppressOOBwarn);  }
	}
	if (use.dxl)
		cout << "WARNING: Attempted to move non-existent servo # " << id << endl;
	return false;

}

// Move servo with ID id to its maximum position
bool DynamixelController::resetServo(int id) {
	for (int i = 0; i < Servos.size(); i++) {
		if (Servos[i]->getId() == id) { return Servos[i]->goToMaximum(true); }
	}
	if (use.dxl)
		cout << "WARNING: Attempted to move non-existent servo # " << id << endl;
	return false;
}

bool DynamixelController::resetServoToMin(int id) {
	for (int i = 0; i < Servos.size(); i++) {
		if (Servos[i]->getId() == id) { return Servos[i]->goToMinimum(true); }
	}
	if (use.dxl)
		cout << "WARNING: Attempted to move non-existent servo # " << id << endl;
	return false;
}


// Move servos by key press
bool DynamixelController::moveServoByKey(int &keynum, bool keytest) {

	switch (keynum) {

	case keyArrowUp: 
		if (!keytest) { moveServo(1, man_int, DXL_RELATIVE,true); cout << "Moved servo 1 to " << positionOfServo(1) << endl; keynum = -5; }
		return true;	/// raise motor 1

	case keyArrowDwn: 
		if (!keytest) { moveServo(1, -man_int, DXL_RELATIVE);  cout << "Moved servo 1 to " << positionOfServo(1) << endl; keynum = -5; }
		return true;	/// lower motor 1

	case keyArrowRight: 
		if (!keytest) { moveServo(2, man_int, DXL_RELATIVE);  cout << "Moved servo 2 to " << positionOfServo(2) << endl; keynum = -5; }
		return true;	/// increase angle on motor 2

	case keyArrowLeft: 
		if (!keytest) { moveServo(2, -man_int, DXL_RELATIVE);  cout << "Moved servo 2 to " << positionOfServo(2) << endl; keynum = -5; }
		return true;	/// decrease angle on motor 2

	case keyOpenSquare:
		if (!keytest) { moveServo(3, -man_int, DXL_RELATIVE);  cout << "Moved servo 3 to " << positionOfServo(3) << endl; keynum = -5; }
		return true;	/// decrease angle on motor 3

	case keyCloseSquare:
		if (!keytest) { moveServo(3, man_int, DXL_RELATIVE);  cout << "Moved servo 3 to " << positionOfServo(3) << endl; keynum = -5; }
		return true;	/// increase angle on motor 3

	case keyPlus:				

		/// switch servo increment 10 or 25
		if (man_int == 2 && !keytest) { man_int = 25; keynum = -5; }
		else if (man_int == 25 && !keytest) { man_int = 2; keynum = -5; }

		/// alert user
		if (!keytest) { cout << "Servo increment switched to " << man_int << endl; }
		return true;
	}
	return false;
}

// Getters

int DynamixelController::numberOfServos() { return (int) Servos.size();  }

int DynamixelController::positionOfServo(int id) {

	for (int i = 0; i < Servos.size(); ++i) {
		if (Servos[i]->getId() == id) {
			return Servos[i]->getPosition();
		}
	}
	return -1;

}