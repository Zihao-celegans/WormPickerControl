/*
	ActuonixController.cpp
	Peter Bowlin
	July 2021
*/

#include "ActuonixController.h"
#include "AnthonysCalculations.h"
#include "AnthonysTimer.h"
#include "Config.h"
#include "json.hpp"
#include "ThreadFuncs.h"

using namespace std;
using json = nlohmann::json;

ActuonixController::ActuonixController(HardwareSelector &_use) : use(_use) {

	// Load the file from disk
	json config = Config::getInstance().getConfigTree("actuonix");

	if (use.actuator) {
		current_position = readPosition();
		max_position = config["max position"];
		min_position = config["min position"];
		//focus_position = config["focus position"];
	}

	return;
}

// Move the actuator to the lowest position and then back up to the highest position
// We've had issues where the actuator fails and loses track of its position. By forcing it down to the lowest position
// it will reset its internal position and ensure that its tracking is correct when we start the program.
// i.e. If the actuator THINKS it is at 600, but it is ACTUALLY at 1700, then when we move it to the max position (currently 1900).
// It will lower 200 units to an actual position of 1900 where it will get blocked from lowering any further by the plate it is attached to.
// Even though it gets blocked, the actuator has no internal sensing to see if it moved the correct amount, so it will think it continued
// moving the other 1100 remaining units and reset its internal sensor to the appropriate value of 1900 units. 
void ActuonixController::homeActuonix() {
	moveAbsolute(max_position);
	moveAbsolute(min_position);
}

// Moves the actuator to an absolute linear position. 
// The HIGHER the position value, the lower the physical position of pick will be
// Note that if you don't block on this motion you should probably verify the position of the actuator seperately on your own to ensure it moved.
bool ActuonixController::moveAbsolute(int position, bool blockflag, int waittime) {
	if (position > max_position || position < min_position) {
		cout << "WARNING: Actuator position out of range!" << endl;
		return false;
	}

	string command_str = "ticcmd -p ";
	command_str += to_string(position);
	const char *command = command_str.c_str(); // Convert string to char pointer for system call
	system(command);
	AnthonysTimer wait_timer;
	int status_pos = current_position;
	bool move_success = true;
	while (blockflag && wait_timer.getElapsedTime() < waittime) { 
		status_pos = readPosition();
		if (status_pos == position) {
			move_success = true;
			break;
		}
		boost_sleep_ms(50); 
	}

	current_position = position;

	return move_success;

}

bool ActuonixController::moveRelative(int position, bool blockflag, int waittime) {
	return moveAbsolute(current_position + position, blockflag, waittime);
}

// Reads the current position from the linear actuator. Will return -1 if we failed to read from the actuator.
int ActuonixController::readPosition() {
	int queried_position = -1;
	string result = readFromStatus("Current position");

	if (result != "NOT FOUND") {
		queried_position = stoi(result);
	}
	//cout << "Queried position from linear actuator: " << queried_position << endl;
	return queried_position;
}

// Reads the value of the specified lookup parameter from the status. 
// Will return "NOT FOUND" if there is a problem with the status or if the lookup parameter was not found.
string ActuonixController::readFromStatus(string lookup) {
	string value_str = "NOT FOUND";
	string result = executeExternal("ticcmd --status");

	if (result != "NOT FOUND") {
		//Extract the value from the returned status
		int idx = result.find(lookup);
		if (idx != -1) {
			idx = result.find_first_not_of(" ", idx + lookup.length() + 1);
			int idx_n = result.find("\n", idx);
			value_str = result.substr(idx, idx_n - idx);
		}
	}
	else {
		cout << "WARNING: Failed to read status from the actuator!" << endl;
	}

	return value_str;
}

