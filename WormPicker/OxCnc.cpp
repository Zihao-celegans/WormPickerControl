/*
	OxCnc.cpp
	Anthony Fouad

	Class definition for OX CNC robot control.
*/

#include "OxCnc.h"

// Standard includes
#include <string>
#include <iostream>
#include <fstream>

// Anthony's Includes
#include "AnthonysKeyCodes.h"
#include "AnthonysCalculations.h"
#include "ImageFuncs.h"
#include "ThreadFuncs.h"
#include "stdafx.h"
#include "ControlledExit.h"
#include "Config.h"
#include "json.hpp"
#include "mask_rcnn.h"

// LabJack Includes
#include "c:\program files (x86)\labjack\drivers\LabJackUD.h" 
#include "LabJackFuncs.h"


// Namespaces
using namespace std;
using namespace cv;
using json = nlohmann::json;

// Initialize static variables
bool OxCnc::move_secondary_manual = false;


//Constructor
OxCnc::OxCnc(string device_name, ErrorNotifier &ee, HardwareSelector& _use, bool iug, bool block_initial_homing, OxCnc* grbl2_ptr) :
	device_name(device_name),
	Err(ee),
	is_upper_gantry(iug),
	use(_use),
	//lid_is_raised(false),
	grbl2_ptr(grbl2_ptr)
{
	// Read in port name 
	cout << "Connecting to OX CNC via Serial Port...\n";
	cout << "\tReading port name...\t";

	json config = Config::getInstance().getConfigTree(device_name);
	
	port_name = config["port"].get<string>();
	cout << port_name << endl;
	man_int = 5;

	// Connect to LabJack (suction control and other functions)

	if (use.lj) {

		// Boot labjack
		int ec1 = LabJack_Bootup(&lab_jack_handle);
		string ec2 = LabJack_SetDacN(lab_jack_handle, 0, 0);

		//Verify it opened and set, although if aborting was enabled in the LabJack error handler this will not be reached. 
		controlledExit(ec1 == 0 && ec2.empty(), "Failed to boot labjack. Program will abort.");

		// Determine whether it gets turned off at shutdown
		Config::getInstance().loadFiles();
		nlohmann::json tree = Config::getInstance().getConfigTree("acquisition parameters");
		//turn_off_red_after = Config::getInstance().readVal(tree, "turn off red after", true);

		// Set all labjack pins to 0
		// It's not possible to avoid this, even if the user setting is leave red on, because
		// Connecting LabJack always sets all pins to 0 somewhere low level. 
		LabJack_SetDacN(lab_jack_handle, 0, 0);
		LabJack_SetDacN(lab_jack_handle, 1, 0);

		for (int i = 0; i < 16; i++) {
			LabJack_SetFioN(lab_jack_handle, i, 0);
			
			if (i == 4 || i == 5 //Turn off vaccum
				|| i == 9 || i == 11 //Upper the lid handler
				) LabJack_SetFioN(lab_jack_handle, i, 1);

		}

		//LabJack_SetFioN(lab_jack_handle, 7, 0); // Turn OFF vaccum the LED light
		//LabJack_SetDacN(lab_jack_handle, 1, 5); // Turn on the LED light
		LabJack_SetFioN(lab_jack_handle, 7, 1); // Turn on the LED light
	}

	else {
		cout << "----------------- LABJACK IS DISABLED ----------------------" << endl;
	}


	//// Initialize current position to zero's
	//pos = Point3d(0, 0, 0);

	// Set up flag variables
	waitingforkeypress = false;

	// Initialize maximum speeds
	max_speed.push_back(7500);
	max_speed.push_back(7500);
	max_speed.push_back(500);
	current_speed = 1;
	jog_speed = 500;

	// Initialize lid-lifter relative position
		/// 2 step operation, first move (RELATIVE) to pos_shift_lid_up,
		/// Then move (RELATIVE) to pos_shift_lid;
		/// Use reverse order to move away

	/// 1 step version
	//json lid_grabber_config = Config::getInstance().getConfigTree("lid grabber");
	////pos_lid_shift = Point3d(-20, -7, 0);
	//pos_lid_shift.x = lid_grabber_config["x offset"];
	//pos_lid_shift.y = lid_grabber_config["y offset"];
	//pos_lid_shift.z = lid_grabber_config["z absolute"];
	//z_lid_height = -60;

	x_sync_offset = config["offset"].get<double>();

	pos_min.x = config["x"]["min"].get<double>();
	pos_min.y = config["y"]["min"].get<double>();
	pos_min.z = config["z"]["min"].get<double>();

	pos_max.x = config["x"]["max"].get<double>();
	pos_max.y = config["y"]["max"].get<double>();
	pos_max.z = config["z"]["max"].get<double>();

	pos_exit.x = config["exit"]["x"].get<double>();
	pos_exit.y = config["exit"]["y"].get<double>();
	pos_exit.z = config["exit"]["z"].get<double>();

	// Initialize current position to the exit position (homing position)
	if (is_upper_gantry) { // Upper gantry only has connections to y and z axes, so initialize x axis to exit position
		pos.x = config["exit"]["x"].get<double>();
		pos.y = 0;
		pos.z = 0;
	}
	else { // Lower gantry only has connections to x and z axes, so initialize y axis to exit position
		pos.x = 0;
		pos.y = config["exit"]["y"].get<double>();
		pos.z = 0;
	}
	// read each stored position
	for(json position : config["stored positions"]){
		int idx = position["Fnkey"].get<int>() - 1;
		Point3d pt;
		pt.x = position["x"].get<double>();
		pt.y = position["y"].get<double>();
		pt.z = position["z"].get<double>();
		stored_positions[idx] = pt;
	}

	// Connect to the serial port (sets isconnected)
	connectToSerial();
	move_xy_first = false;

	// Continue serial operations only if connection was successful. If not, and it was supposed to be, error
	if (isconnected) {
		// Home the CNC (forced this time, but normally won't home if done < 2 min ago).
		homeCNC(true, block_initial_homing); /// Force flag set to true, home even if done recently
		skip_homing_if_s_ago = 60*10;
	}
	else if (use.grbl) {
		Err.notify(errors::grbl_disconnect);
	}
}


// Destructor
OxCnc::~OxCnc() {

	// Go to terminate to the home position
	if (isconnected) {
		OxCnc::abortJog();
		if (is_upper_gantry) { raiseGantry(); }
		goToAbsolute(pos_exit, true);
		boost_sleep_ms(2000);
	}

	// Set all LabJack stuff to low
	if (use.lj && turn_off_red_after) {
		LabJack_SetDacN(lab_jack_handle, 0, 0);
		LabJack_SetDacN(lab_jack_handle, 1, 0);
		LabJack_SetFioN(lab_jack_handle, 0, 0);
		LabJack_SetFioN(lab_jack_handle, 1, 0);
		LabJack_SetFioN(lab_jack_handle, 7, 0);
	}

	// Free the heap memory allocated to the serial object
	delete Serial;
}

// Run homing sequence and wait for it to finish (block until finish)
void OxCnc::homeCNC(bool force, bool blockflag) {

	// Alert user whether we will home or skip. Don't skip if forced homing is specified
	if (T_last_home.getElapsedTime() < skip_homing_if_s_ago && !force) {
		Err.notify(errors::homing_skip);
		return;
	}
	else {
		Err.notify(errors::waiting_for_homing);
	}

	// Keep track of how long we've tried to home and which attempt this is
	int tries = 1, max_tries = 2;
	AnthonysTimer Ta;
	double max_home_time = 120;


	do {

		// Start homing timer
		Ta.setToZero();
		grbl_error.clear();

		// Reconnect if this is the second attempt at homing
		if (tries > 1)
			reconnectSerial();

		// Trigger homing
		is_homing = true;
		Err.notify(LOG_ONLY,"Sending homing command on port: " + port_name + "...");
		Serial->writeString("$H\r");

		// Wait for homing to finish up to 120 s
		boost_sleep_ms(100);

		/// CAUTION: GRBL does not send feedback during homing and responses 
		/// Can falsely indicate that device is Idle. One solution is the reset
		/// The GRBL before rehoming (ctrl+X). This flushes the buffer of stale 
		/// Feedback which might or might not say idel.

	// Wait for CNC to report idle or time to expire or an error 
		while (!isInState("Idle", true) && Ta.getElapsedTime() < max_home_time && grbl_error.empty() && blockflag) { boost_sleep_ms(200); }

	// Test-move to the back right corner, but return immediately so we can monitor and wait here. 
	// This also avoids recursion on failed moves because blockflag checking is not used.
		if (grbl_error.empty()) {
			goToAbsolute(pos_exit, false);

			// Wait for movement to finish for up to 15 seconds.
			AnthonysTimer Tb;
			boost_sleep_ms(100);
			while (isMoving() && Tb.getElapsedTime() < 15 && Ta.getElapsedTime() < max_home_time && grbl_error.empty()) { boost_sleep_ms(200); }
		}

		// Log in the notes if homing has failed 
		if (Ta.getElapsedTime() >= max_home_time || !grbl_error.empty()) {
			Err.notify(errors::homing_attempt_fail);
		}

	} while (tries++ < max_tries && (Ta.getElapsedTime() >= max_home_time || !grbl_error.empty()));

	// Verify that homing completed within 120 seconds and no errors occurred
	if (Ta.getElapsedTime() >= max_home_time || !grbl_error.empty()) {
		Err.notify(errors::homing_error);
	}
	else {
		Err.notify(errors::homing_completed);
	}

	// Signal done and reset skip-homing timer to 0
	is_homing = false;
	T_last_home.setToZero();
}

// Check that the CNC is connected. If not, reconnect and re-home, blocking all execution until finished. 
///		WARNING: connection will be reported invalid if the GRBL is unable to immediately return a response, such as
///		during homing and alarm states

void OxCnc::validateConnection() {

	/// Request a response
	AnthonysTimer Ta;
	while (!isAnything() && Ta.getElapsedTime() < 5) { boost_sleep_ms(25); }

	/// If the grbl failed to responsd, reconnect and rehome
	if (Ta.getElapsedTime() >= 5) {
		Err.notify(errors::failed_grbl_repsonse);
		/// Restart
		int tries = 0;
		while (!reconnectSerial()) {
			boost_sleep_ms(500);
			cout << " " << ++tries;
		}

		/// Rehome
		homeCNC();

		/// If we got here it's working correctly, proceed with requested command
		Err.notify(PRINT_ONLY, "Connected and re-homed. Proceeding with command...");
	}

}

bool OxCnc::connectToSerial() {

	// Log
	Err.notify(LOG_ONLY, "Connecting to the serial port" + port_name);

	// Allocate a blank serial object
	Serial = new BufferedAsyncSerial();

	// Attempt to connect to the serial port
	baud_rate = 115200;
	isconnected = false;

	try {
		if (use.grbl) {
			// Allocate and construct temporary serial connection
			Err.notify(LOG_ONLY, "Creating object for port: " + port_name);
			BufferedAsyncSerial *temp = new BufferedAsyncSerial(port_name, baud_rate);

			// If completed successfully, delete the memory allocated to the blank object and transfer 
			// the temp object's heap memory to the main object
			delete Serial;
			Serial = temp;

			// Wait 2 seconds before continuing, takes OX a moment to boot
			boost_sleep_ms(2000);

			// Display all data
			Serial->readStringAllLines();

			// Indicate that the connection was successful
			isconnected = true;
			Err.notify(PRINT_ONLY,"Successfully connected to " + port_name);
		}
		else {
			Err.notify(PRINT_ONLY, "GRBL has been deactivated by settings file.");
		}
	}
	catch (exception e1) {
		Err.notify(errors::grbl_disconnect);
	}

	return isconnected;

}

bool OxCnc::reconnectSerial() {

	bool is_reconnected = true;

	if (use.grbl && isConnected()) {

		// Delete the serial object to release it
		delete Serial;
		isconnected = false;

		// Connect again, wait a moment
		boost_sleep_ms(3000);
		is_reconnected = connectToSerial();
		boost_sleep_ms(3000);

		// Exit if we failed to reconnect
		if (!is_reconnected) 
			Err.notify(errors::grbl_disconnect);
		
	}

	return is_reconnected;

}

// Write numeric positions to grbl and wait. No error check. Do not call directly. 

void OxCnc::writeSerialPos(double x, double y, double z, bool blockflag, int waitTime) {

	// Write position
	boost_sleep_ms(14);
	string to_send = "X" + to_string(x) + "Y" + to_string(y) + "Z" + to_string(z) + "\r";
	if (use.grbl) {
		Err.notify(LOG_ONLY,"Writing serial string on port" + port_name + ": " + to_send);
		Serial->writeString(to_send);
		Err.notify(LOG_ONLY, "Finished sending string");
	}
		
	

	// If the user wants to wait until the machine is done moving. 
	///	Any error on any wait iteration will set the grbl_error string to non-empty. 
	if (blockflag) {
		Err.notify(LOG_ONLY, "Blocking until position matches request...");
		AnthonysTimer Ta;
		boost_sleep_ms(100);
		while (isMoving() && Ta.getElapsedTime() < waitTime) { boost_sleep_ms(200); } /// Wait for it to finish moving
		//cout << "Waited for " << Ta.getElapsedTime() << " s\n";
		boost_sleep_ms(50);
		Err.notify(LOG_ONLY, "Finished blocking.");
		
	}
}


// Go to specific coordinates (X, Y, Z as arguments)
bool OxCnc::goToAbsolute(double x, double y, double z, bool blockflag, int waitTime) {
	//cout << is_upper_gantry << " moving: " << x << ", " << y << ", " << z << endl;
	AnthonysTimer block_timer;
	// The old wormpicker motor set up had the upper gantry control X,Y,Z of the robot, and the lower gantry controlled the X of the illumination bar and the Z of the picking arm.
	// The new wormpicker 2 set up has the upper gantry controlling Y and Z of the robot, and the lower gantry controlling the X of both the robot and the illumination bar. The Z of the picking arm is now
	// controlled via a linear actuator. 
	// To rectify the differences between the set ups without needing to change every place we have a movement command in the code we now take any X commands given to the upper gantry (as per the wormpicker 1
	// set up) and route them down to the lower gantry for execution.
	if (is_upper_gantry && grbl2_ptr) {
		double desired_x = x; 
		//x = getX();

		grbl2_ptr->goToAbsolute(desired_x, grbl2_ptr->getY(), desired_x, false, waitTime); // Grbl2 axes are X_upper_gantry, Nothing, X_lower_gantry. We want the upper and lower X to always be synchronized.
	}
	else if (!is_upper_gantry) {
		z = z - x_sync_offset;
	}

	// Make sure we are connected
	// validateConnection();
	// Verify that the position is in-bounds of [xMin,xMax,yMin,yMax,zMin,zMax]
	bool is_good = x >= pos_min.x && x <= pos_max.x && y >= pos_min.y
				&& y <= pos_max.y && z >= pos_min.z && z <= pos_max.z;

	if (!is_good) {
		Err.notify(errors::coord_oob);
		cout << "Targeted position (x,y,z): " << Point3d(x, y, z) << endl;
		pos_goal = pos;
		return false;
	}
	else {
		pos_goal = Point3d(x, y, z);
	}

	// Keep track of how many times we've tried this move. If errors are detected it will home and retry
	int tries = 1, max_tries = 2; /// Tries is how many times we tried to make THIS movement specifically
	bool error_on_this_move = false; /// Whether a movement error has been detected on THIS movement specifically.
							///		Example: Rehoming is successful but the position is just out of bounds.
							///		This sould be recognized as 2 tries to make the same failed move.
							///		If Grbl error is used directly, it will infinitely loop in 1 try because
							///		The rehoming counts as a successful move. 
	string email_msg(""); /// Cumulative email message for all tries

	do {

		// Reset error detection. If blocking is requested, we will listen for errors. 
		grbl_error.clear();
		error_on_this_move = false;

		// Abort any jogging actions
		boost_sleep_ms(5);
		abortJog();

		// Special case of recovering from a multistep move: first go to X and Y at the maximum position.
		if (move_xy_first) {
			writeSerialPos(x,y,pos_max.y,blockflag,waitTime);
			move_xy_first = false;
		}
		// Send position string to the OX's serial object
		writeSerialPos(x,y,z,blockflag,waitTime);
		

		// If necessary, wait for the grbl2 to finish moving in the x direction
		if (is_upper_gantry && grbl2_ptr && blockflag) {
			while (grbl2_ptr->isMoving() && block_timer.getElapsedTime() < waitTime) { boost_sleep_ms(200); } /// Wait for it to finish moving
		}

		/*cout << is_upper_gantry << " block flag: " << blockflag << endl;
		cout << is_upper_gantry << " grbl error empty: " << grbl_error.empty() << endl;
		cout << is_upper_gantry << " verify grbl pos target: " << Point3d(x, y, z) << endl;*/

		// If waiting is requested, and no error was found so far, 
		// confirm that the GRBL board reports the correct coordinates.
		if (blockflag && grbl_error.empty() && !verifyGrblPos(Point3d(x, y, z))) {
			
			cout << is_upper_gantry << " In error statement" << endl;
			boost_sleep_ms(2000);
			pos_grbl = queryGrblPos();
			cout << "Error re-query pos: " << pos_grbl << endl;
			string mismatch_pos_error = "Error in OxCnc.cpp: Position on GRBL board is (" +
				to_string(pos_grbl.x) + "," + to_string(pos_grbl.y) + "," + to_string(pos_grbl.z) +
				") Which does not match position requested (" +
				to_string(x) + "," + to_string(y) + "," + to_string(z) + "). Possible causes:\n"
				+ "\t1. CNC is hitting a limit switch when it tries to move here.\n"
				+ "\t2. CNC failed to home.\n"
				+ "\t3. Mismatch between CNC driver limits and user specified limits\n"
				+ "\t4. CNC/Grbl disconnected\n."
				+ "Some problems might be corrected during automatic re-homing, otherwise program will exit after second try\n.";
				
				grbl_error.push_back(mismatch_pos_error);
				cout << mismatch_pos_error << endl;
		}

		// Was any error detected specifically on THIS movement?
		error_on_this_move = !grbl_error.empty();

		// If any error was detected, attempt to fix it by homing the CNC (and run again)
		if (blockflag && error_on_this_move) {
			
			/// Print out the error and attempt to fix
			Err.notify(errors::cnc_move_attempt_fail);
			// email_msg += error_msg;
			if (is_upper_gantry && grbl2_ptr) {
				grbl2_ptr->reconnectSerial();
				grbl2_ptr->homeCNC(true, false);
			}

			reconnectSerial(); /// Reconnect to the serial port in case limit switch was hit (board totally frozen)
			homeCNC(true); /// Re-home the CNC
			tries++;
			move_xy_first = true; /// Signal that on recovery we should FIRST go to the XY position at max height
		}

		// If no error was detected, update current position in the object
		else {
			pos = Point3d(x, y, z);
		}
	} while (error_on_this_move && tries <= max_tries);

	// If we have twice failed to move (either in this call to goToAbsolute() or any other
	// without error, abort. This prevents a well from getting Repeatedly dosed if the CNC 
	// is "stuck" there. Email is sent to user
	if (blockflag && tries > max_tries && error_on_this_move) {
		Err.notify(errors::cnc_move_fail);
	}

	// If we moved successfully, reset tries to 1
	else {	tries = 1; }

	// Return
	//cout << "Time to move: " << block_timer.getElapsedTime() << endl;
	return is_good && grbl_error.empty();
}

// Go to specific coordinates (3-element vector as argument)
bool OxCnc::goToAbsolute(vector<double> newpos, bool blockflag, int waitTime) {
	return goToAbsolute(newpos[0], newpos[1], newpos[2], blockflag, waitTime);
}

// Got to specific coordinates (3d point as argument)
bool OxCnc::goToAbsolute(Point3d newpos, bool blockflag, int waitTime) {
	return goToAbsolute(newpos.x, newpos.y, newpos.z, blockflag, waitTime);
}

// Go to specific coordinates (3d point as argument AND raises gantry before moving AND lowers gantry when finished
bool OxCnc::goToAbsoluteWithRaise(Point3d newpos, StrictLock<OxCnc> &grblLock, bool blockflag, int waitTime) {

	// raise gantry
	raiseGantry();

	// Move to the new position EXCEPT for the Z coordinate
	Point3d temp = Point3d(newpos.x, newpos.y, getZ());
	goToAbsolute(temp, blockflag, waitTime);

	// Move down to the final new position
	return goToAbsolute(newpos, blockflag, waitTime);;
}


// Move relative to the current position
bool OxCnc::goToRelative(double dx, double dy, double dz, double multiplier, bool blockflag, int waitTime) {

	cout << "goToRelative " << dy << endl;
	// Return if no movements are commanded
	if (dx == 0 && dy == 0 && dz == 0) { return false; }

	// Calculate new coordinates
	double x_new = pos.x + dx * multiplier;

	/*double x_new = grbl2_ptr->getX() + dx * multiplier;
	if (!is_upper_gantry) {
		double x_new = pos.x + dx * multiplier;
	}*/

	double y_new = pos.y + dy * multiplier;
	double z_new = pos.z + dz * multiplier;

	// Send coordinates to OX, report whether coords were in bounds
	return goToAbsolute(x_new, y_new, z_new, blockflag, waitTime);

}

bool OxCnc::goToRelative(vector<double> dv, double multiplier, bool blockflag, int waitTime) {
	return goToRelative(dv[0], dv[1], dv[2], multiplier, blockflag, waitTime);
}

bool OxCnc::goToRelative(Point3d dv, double multiplier, bool blockflag, int waitTime){
	return goToRelative(dv.x, dv.y, dv.z, multiplier, blockflag, waitTime);
}

// Move using keyboard hotkeys
bool OxCnc::goToByKey(const int &keynum, StrictLock<OxCnc> &grblLock, bool block_flag) {

	// Reject any new commands if the GRBL is still moving
	if (isMoving())
		return false;
	
	// Relative movments
	switch (keynum) {


	case 50: setRelSpeed(1); goToRelative(0, -1.0*man_int, 0, 1, block_flag, 9);  return true; // 2	- down	- y to front
	case 56: setRelSpeed(1); goToRelative(0				, man_int		, 0, 1, block_flag, 9); return true; // 8	- up	- y to back
	case 52: setRelSpeed(1); goToRelative(-1.0 * man_int, 0				, 0, 1, block_flag, 9);  return true; // 4	- left	- x to left
	case 54: setRelSpeed(1); goToRelative(man_int		, 0				, 0, 1, block_flag, 9);  return true; // 6	- right	- x to right
	case 55: setRelSpeed(1); goToRelative(0				, 0				, -1.0 * man_int, 1, block_flag, 9);  return true; // 7	- down	- z down 5 mm
	case 49: setRelSpeed(1); goToRelative(0				, 0				, -1.0 * man_int, 1, block_flag, 9);  return true; // 1	- down	- z down 1 mm
	case 57: setRelSpeed(1); goToRelative(0				, 0				, man_int, 1, block_flag, 9);  return true;	// 9	- up	- z up 5 mm
	case 51: setRelSpeed(1); goToRelative(0				, 0				, man_int, 1, block_flag, 9);  return true;	// 3	- up	- z up 1 mm

	case 48: waitingforkeypress = true; cout << "Press F1 - F4 to save position...\n";   return true;
	case 111: 
		// o - switch movement interval
		if (man_int == 0.25) { man_int = 0.5;  }
		else if (man_int == 0.5) { man_int = 1; }
		else if (man_int == 1) { man_int = 5;  }
		else if (man_int == 5) { man_int = 20;  }
		else if (man_int == 20) { man_int = 50;  }
		else if (man_int == 50) { man_int = 100; }
		else if (man_int == 100) { man_int = 0.25; }
		cout << "OxCnc increment switched to " << man_int << endl;
		return true;
	case 97: abortJog();  return true;
	} 

	// Save a stored absolute position if signalled
	if (waitingforkeypress ) {

		// Figure out which index within pos_stored to edit (-1 if none selected)
		int idx = keyPressIdx(keynum);

		if (idx != -1) {

			// Update one position in pos_stored
			cout << "Storing position F" << idx + 1 << ": [";
			stored_positions[idx] = pos;
			cout << pos.x << " " << pos.y << " " << pos.z;
			cout << "]" << endl;

			// Save all positions to disk, but load latest info first
			Config::getInstance().loadFiles();
			json config = Config::getInstance().getConfigTree(device_name);

			// TODO - this is very sloppy, fix later
			bool wasWritten = false;
			for (json position : config["stored positions"]) {
				if (position["Fnkey"].get<int>() == idx + 1) {
					position["x"] = pos.x;
					position["y"] = pos.y;
					position["z"] = pos.z;
					wasWritten = true;
					break;
				}
			}
			if (!wasWritten) {
				json newPt;
				newPt["Fnkey"] = idx + 1;
				newPt["x"] = pos.x;
				newPt["y"] = pos.y;
				newPt["z"] = pos.z;
				config["stored positions"].push_back(newPt);
			}

			Config::getInstance().replaceTree(device_name, config);
			Config::getInstance().writeConfig();

			waitingforkeypress = 0;
			
		}
	}

	// Go to a stored absolute position 1-4 if F1-F4 is pressed
	else if (keyPressIdx(keynum) > -1){
		goToAbsolute(keyPressPos(keynum), true, 15);
		return true; 
	}

	return false;
}

// Jog the CNC (note: shorter distances = less jogging lag)
bool OxCnc::jogOx(double dx, double dy, double dz) {
	//cout << "jog ox: " << dx << ", " << dy << ", " << dz << endl;
	// Ensure we are still connected
	// validateConnection();

	// Calculate what the new position will be after jogging
	double x_new = pos.x + dx;
	double y_new = pos.y + dy;
	double z_new = pos.z + dz;

	// Verify that the new position is in-bounds of [xMin,xMax,yMin,yMax,zMin,zMax]
	bool is_good = x_new >= pos_min.x && x_new <= pos_max.x && y_new >= pos_min.y
		&& y_new <= pos_max.y && z_new >= pos_min.z && z_new <= pos_max.z;

	if (is_good) {

		// Assign the new position
		pos.x = x_new;
		pos.y = y_new;
		pos.z = z_new;

		// Execute the jog
		string to_send_upper_gantry = "$J=G21G91X" + to_string(dx) + "Y" + to_string(dy) +
			"Z" + to_string(dz) + "F" + to_string(jog_speed) + "\r";

		if(use.grbl){ 
			Err.notify(LOG_ONLY,"Writing jog string on port: " + port_name 
							+ " - " + to_send_upper_gantry);
			Serial->writeString(to_send_upper_gantry); 
		}

		if (is_upper_gantry && grbl2_ptr) {
			// Because grbl 2 controls the X locations on wormpicker 2, we must also Jog on Grbl 2
			string to_send_lower_gantry = "$J=G21G91X" + to_string(dx) + "Y" + to_string(dy) +
				"Z" + to_string(dx) + "F" + to_string(jog_speed) + "\r";

			if (grbl2_ptr->use.grbl) {
				Err.notify(LOG_ONLY, "Writing jog string on port: " + port_name
					+ " - " + to_send_lower_gantry);
				grbl2_ptr->Serial->writeString(to_send_lower_gantry);
			}
			
			// Update the new position of Grbl2
			grbl2_ptr->pos = Point3d(x_new, grbl2_ptr->pos.y, x_new);
		}

		return true;
	}
	else {
		Err.notify(errors::coord_oob);
		cout << "Target point (x,y,z): " << Point3d(x_new, y_new, z_new) << endl;
		cout << "Jog distance (dx, dy, dz): " << Point3d(dx, dy, dz) << endl;
		return false;
	}

}

// Abort a jog
void OxCnc::abortJog() {
	if(use.grbl)
		Serial->write("\x85", 1);
}


// Step the targeted worm towards the pickup location
bool OxCnc::moveTowardTargetedPoint(cv::Point2f tracked_centroid, cv::Point target_point, double track_radius, double jog_interval, int max_allow_offset) {

	// Exit if there is no tracked centroid
	if (tracked_centroid.x < 0) { return false; }

	// Slow down speed if we are terminate to the target
	//double jog_interval = 0.009;
	if (ptDist(tracked_centroid, target_point) < 1.5*track_radius) {
		jog_interval = jog_interval / 3;
	}

	// Step the worm towards the center. Don't bother with very small corrections to avoid oscillations.
	double dy = 0, dx = 0;
	
	if (target_point.y > tracked_centroid.y+max_allow_offset)			{ dy = -jog_interval;  }
	else if (target_point.y < tracked_centroid.y-max_allow_offset)		{ dy = jog_interval; }

	if (target_point.x > tracked_centroid.x+max_allow_offset)			{ dx = -jog_interval; }
	else if (target_point.x < tracked_centroid.x-max_allow_offset)		{ dx = jog_interval;  }

	//cout << "Target: " << target_point << ", Tracked: " << tracked_centroid << ", dy: " << dy << ", dx: " << dx <<endl;

	jogOx(dx, dy, 0);

	// Pause for a moment
	boost_sleep_ms(5);
	return true;
}

// Raise up the gantry (e.g. for pick finding)
bool OxCnc::raiseGantry() {

	// Store the current position
	pos_mem = pos;

	// Go up
	return goToAbsolute(pos.x, pos.y, pos_max.z,true,10);
}

// Lower the gantry back to its original position IF a memory position was saved
bool OxCnc::lowerGantry() {

	/// If memory position exists, move to it and erase it
	if (pos_mem) {
		int return_val = goToAbsolute(*pos_mem,true);
		pos_mem == boost::none;
		return return_val;
	}

	/// If no position exists, return nothing
	else {
		return false;
	}
}

//// CNC Motions associated with Lid lifting
//void OxCnc::liftOrLowerLid(bool lifting) {
//
//	//// Make sure lid isn't already in the air
//	//if (lid_is_raised || !is_upper_gantry) {
//	//	Err.notify(errors::lid_lift_fail);
//	//	boost_sleep_ms(10);
//	//	return;
//	//}
//
//	cout << "Liftlid here!" << endl;
//
//	// Lower the gripper
//	LabJack_SetFioN(lab_jack_handle, 9, 0);
//	boost_sleep_ms(1500);
//	// Activate/deactivate suction
//	if (use.lj) {
//		//LabJack_SetDacN(lab_jack_handle, lj.vac_dac, 5);
//		int mode = lifting ? 0 : 1;
//		LabJack_SetFioN(lab_jack_handle, 4, mode);
//		LabJack_SetFioN(lab_jack_handle, 5, mode);
//	}
//
//	boost_sleep_ms(500);
//	// Raise lid gripper
//	LabJack_SetFioN(lab_jack_handle, 9, 1);
//	boost_sleep_ms(500);
//}
//
//// CNC motions associated with lid lifting
//void OxCnc::lowerLid(bool return_to_plate) {
//
//	// Make sure lid is already in the air
//	if (!lid_is_raised || !is_upper_gantry) {
//		Err.notify(errors::lid_lower_fail);
//		return;
//	}
//
//	// Replace lid 
//	boost_sleep_ms(25);							/// Wait
//	goToAbsolute(pos_lid, true);						/// Go back to lid X,Y position				
//	goToAbsolute(pos.x, pos.z, z_lid_height, true); /// Go back to lid DOWN position
//	if (use.lj)
//		LabJack_SetDacN(lab_jack_handle, lj.vac_dac, 0);		/// Release suction
//	boost_sleep_ms(500);						/// Wait6
//
//	if (return_to_plate)
//		goToAbsolute(pos_plate, true);					/// Return to camera position
//
//	lid_is_raised = false;
//}

// Bring the lower gantry into synchrony with the upper gantry
void OxCnc::synchronizeXAxes(double OxX) {

	// Synchronization should only be perfored for Grbl2, Ox's sync offset should be 0
	double target_pos = OxX + x_sync_offset;

	/* Synchronize unless:
			- target pos is OOB
			- this is an UPPER gantry (also, the offset should be 0 for upper anyway)
			- grbl is disabled
			- the X axis is already at the correct position
	*/
	double diff = abs(target_pos - getX());

	if (use.grbl && !is_upper_gantry &&  target_pos >= pos_min.x && target_pos <= pos_max.x && diff > 0.4)
		goToAbsolute(target_pos, getY(), getZ(), false);

}

// (During move secondary manual mode), change the offset and save to disk
void OxCnc::updateOffset(double OxX) {
	
	// do nothing if this is an upper gantry
	if (is_upper_gantry)
		return;
	
	// Calculate the current offset (OxX should be more positive than Grbl2X, such that offset is positive)
	x_sync_offset = getX() - OxX;

	writeGrblLimits();
}

// Check current GRBL state and update return str
bool OxCnc::isInState(string state_str,string& return_str, bool update_alarm) {
	
	bool in_state = true;

	// Write status character to serial, then query repeatedly for up to 
	// 0.25 seconds until we get a valid state string back.
	string s("");
	int tries = 0;
	Err.notify(LOG_ONLY,"Sending query command on port: " + port_name + "...");
	Serial->writeString("?");

	while (s.find("<") == string::npos && tries++ < 6) {
		
		boost_sleep_ms(200);

		// Read in response and store it in local variable
		Err.notify(LOG_ONLY, "Reading response - " + port_name + 
					" - try " + to_string(tries) + "...");
		Serial->readStringAllLines();
		s = Serial->readRecent();
		Err.notify(LOG_ONLY, s);

		// Check if this string has GRBL alarms or errors EVEN if it isn't a valid state
		if ((s.find("ALARM") != string::npos || s.find("ERROR") != string::npos) && update_alarm) {
			grbl_error.push_back(s);
			grbl_error.push_back(translateGrblCode(s)); /// Translate the error as well
		}
	}



	// Determine whether the state matches the requested string
	if (s.find(state_str) != string::npos) {
		in_state = true;
	}
	else {
		in_state = false;
	}

	// If return str reference was supplied, store the message here. 
	return_str = s;

	return in_state;
}

// Check current GRBL state only (do not update return_str)
bool OxCnc::isInState(std::string state_str, bool update_alarm) {
	string trash("");
	return isInState(state_str, trash, update_alarm);
}

bool OxCnc::isMoving() {
	return isInState("Run");
}

bool OxCnc::isDoneHoming() {

	return isInState("Idle"); // Will be idle when done homing
}

bool OxCnc::isAnything() {
	bool testval = isInState("Home") || isInState("Idle") || isInState("Run");
	return testval;
}


// Getter functions
double OxCnc::getX() const { return pos.x; }
double OxCnc::getY() const { return pos.y; }
double OxCnc::getZ() const { return pos.z; }
double OxCnc::getXgoal() const { return pos_goal.x; }
double OxCnc::getYgoal() const { return pos_goal.y; }
double OxCnc::getZgoal() const { return pos_goal.z; }
Point3d OxCnc::getXYZ() const { return  pos; }
Point3d OxCnc::getXYZgoal() const { return  pos_goal; }
double OxCnc::getInt() const { return man_int; }
long OxCnc::getLabJackHandle() const { return lab_jack_handle; }
bool OxCnc::isConnected() const { return isconnected; }
//bool OxCnc::getLidIsRaised() const { return lid_is_raised; }
//Point3d OxCnc::getPosLidShift() const { return pos_lid_shift; }
//double OxCnc::getZPlateHeight() const { return z_lid_height; }

Point3d OxCnc::queryGrblPos() {

	// Get the current state string from GRBL
	// If we failed to get a string (e.g. this is not a status report) , try a few more times to be sure.
	string grbl_msg;
	int tries = 1;
	while (tries++ < 3 && !isInState("MPos:", grbl_msg, false)) { 
		boost_sleep_ms(10); 
		Err.notify(errors::failed_grbl_read);
	}


	// If completely failed to get a position string, return 1,1,1, a not-found signal
	if (!isInState("MPos:", grbl_msg, false))
		return Point3d(1, 1, 1);

	// Get the location within the string that contains MPos
	int idx = (int) grbl_msg.find("MPos");
	int L = (int) grbl_msg.size() - 1 - idx;
	string cropped = grbl_msg.substr(idx, L);

	// Read in all numbers from the string after MPos
	vector<double> all_nums = string2double(cropped);

	// If failed to find 3+ numbers in the string, return 2,2,2 a not-found signal
	if (all_nums.size() < 3)
		return Point3d(2, 2, 2);

	// Convert first 3 nums to 3d point and return
	else
		return Point3d(all_nums[0], all_nums[1], all_nums[2]);
	

}

bool OxCnc::verifyGrblPos(Point3d test_pt, double precision) {

	// Query the grbl position. (1,1,1) is the signal that no position was returned. 
	// If invalid, try a couple more times in case it is still finishing up the movement.
	bool is_valid = false;
	int tries = 0;
	while (!is_valid && tries < 4) {
		// TODO: Failing to validate when moving with script action. Look into Query grbl pos.
		pos_grbl = queryGrblPos();
		is_valid = abs(test_pt.x - pos_grbl.x) < precision && pos_grbl.x != 1 &&
			abs(test_pt.y - pos_grbl.y) < precision && pos_grbl.y != 1 &&
			abs(test_pt.z - pos_grbl.z) < precision && pos_grbl.z != 1;
		if (!is_valid) {
			(errors::position_mismatch);
			boost_sleep_ms(250);
		}
		tries++;
	}
	return is_valid;
}

void OxCnc::writeGrblLimits() {

	// Load the JSON file
	Config::getInstance().loadFiles();
	json config = Config::getInstance().getConfigTree(device_name);
	
	// Abort writing if all is up to date anyway
	if (config["offset"] == x_sync_offset &&
		config["x"]["min"] == pos_min.x &&
		config["y"]["min"] == pos_min.y &&
		config["z"]["min"] == pos_min.z &&
		config["x"]["max"] == pos_max.x &&
		config["y"]["max"] == pos_max.y &&
		config["z"]["max"] == pos_max.z
		) {
		return;
	}

	config["offset"] = x_sync_offset;

	config["x"]["min"] = pos_min.x;
	config["y"]["min"] = pos_min.y;
	config["z"]["min"] = pos_min.z;

	config["x"]["max"] = pos_max.x;
	config["y"]["max"] = pos_max.y;
	config["z"]["max"] = pos_max.z;

	Config::getInstance().replaceTree(device_name, config);
	Config::getInstance().writeConfig();

}

bool OxCnc::isRecognizedKey(int k) {
	vector<int> valid_keys = { 50,56,52,54,55,49,57,51,48,103,111,97, keyF1, keyF2, keyF3, keyF4, keyHome };
	return find(valid_keys.begin(), valid_keys.end(), k) != valid_keys.end();
}
bool OxCnc::isHoming() const { return is_homing; }
// Setter functions
void OxCnc::setRelSpeed(double speed) {

	// Change current speed
	if (speed > 0 && speed <= 1) { current_speed = speed; }
	else { cout << "Failed to set OxCnc Speed...\n"; }

	// Set speed on X and Y axes only (Z would be i<3) according to current speed
	string to_send;
	for (int i = 0; i < 2; i++) {
		to_send = "$" + to_string(110 + i) + "=" + to_string((int)(current_speed * max_speed[i])) + "\r";
		if (use.grbl) {
			Err.notify(LOG_ONLY,"Sending speed setting: " + to_send + "...");
			Serial->writeString(to_send);
			Err.notify(LOG_ONLY, "Finished sending string.");
		}
			
		boost_sleep_ms(14);
	}

}

// Figure out which 3 elements in pos_stored to edit, depending on which key F1-F4 was pressed. If none, return -1
int OxCnc::keyPressIdx(int keynum) {

	int idx = -1;
	if (keynum == keyF1) { idx = 0; }
	else if (keynum == keyF2) { idx = 1; }
	else if (keynum == keyF3) { idx = 2; }
	else if (keynum == keyF4) { idx = 3; }

	return idx;
}

// Extract a 3-element vector corresponding to whichever key was pressed

Point3d OxCnc::keyPressPos(int keynum) {

	// figure out which index to extract from
	int idx = keyPressIdx(keynum);

	return stored_positions[idx];

}


string OxCnc::formatGrblErrors() {

	string all_errors("GRBL Errors:\n");

	for (int i = 0; i < grbl_error.size(); i++)
		all_errors += (grbl_error[i] + "\n");
	all_errors += "[End of GRBL errors]\n";

	return all_errors;
}

string OxCnc::translateGrblCode(string test_str) {

	// Determine if an error or alarm exists in the string, return if not.
	bool is_alarm = test_str.find("ALARM:") != string::npos;
	bool is_error = test_str.find("error:") != string::npos;
	if (!is_alarm && !is_error) { return ""; }

	// Parse the alarm code in the string
	vector<double> all_nums = string2double(test_str);
	int code = 0;
	if (all_nums.size() > 0)
		code = (int) all_nums[0];
	else
		return "Failed to parse string when translating grbl code!";

	// Retrieve correct error or ALARM code translation
	if (is_error) {

		switch (code) {
		case 1:
			return ("G-code words consist of a letter and a value. Letter was not found.");
			break;
		case 2:
			return("Numeric value format is not valid or missing an expected value.");
			break;
		case 3:
			return("Grbl '$' system command was not recognized or supported.");
			break;
		case 4:
			return("Negative value received for an expected positive value.");
			break;
		case 5:
			return("Homing cycle is not enabled via settings.");
			break;
		case 6:
			return("Minimum step pulse time must be greater than 3usec");
			break;
		case 7:
			return("EEPROM read failed. Reset and restored to default values.");
			break;
		case 8:
			return("Grbl '$' command cannot be used unless Grbl is IDLE. Ensures smooth operation during a job.");
			break;
		case 9:
			return("G-code locked out during alarm or jog state");
			break;
		case 10:
			return("Soft limits cannot be enabled without homing also enabled.");
			break;
		case 11:
			return("Max characters per line exceeded. Line was not processed and executed.");
			break;
		case 12:
			return("(Compile Option) Grbl '$' setting value exceeds the maximum step rate supported.");
			break;
		case 13:
			return("Safety door detected as opened and door state initiated.");
			break;
		case 14:
			return("(Grbl-Mega Only) Build info or startup line exceeded EEPROM line length limit.");
			break;
		case 15:
			return("Jog target exceeds machine travel. Command ignored.");
			break;
		case 16:
			return("Jog command with no '=' or contains prohibited g-code.");
			break;
		case 17:
			return("Laser mode requires PWM output.");
			break;
		case 20:
			return("Unsupported or invalid g-code command found in block.");
			break;
		case 21:
			return("More than one g-code command from same modal group found in block.");
			break;
		case 22:
			return("Feed rate has not yet been set or is undefined.");
			break;
		case 23:
			return("G-code command in block requires an integer value.");
			break;
		case 24:
			return("Two G-code commands that both require the use of the XYZ axis words were detected in the block.");
			break;
		case 25:
			return("A G-code word was repeated in the block.");
			break;
		case 26:
			return("A G-code command implicitly or explicitly requires XYZ axis words in the block, but none were detected.");
			break;
		case 27:
			return("N line number value is not within the valid range of 1 - 9,999,999 .");
			break;
		case 28:
			return("A G-code command was sent, but is missing some required P or L value words in the line.");
			break;
		case 29:
			return("Grbl supports six work coordinate systems G54-G59 . G59.1 , G59.2 , and G59.3 are not supported.");
			break;
		case 30:
			return("The G53 G-code command requires either a G0 seek or G1 feed motion mode to be active.A different motion was active.");
			break;
		case 31:
			return("There are unused axis words in the block and G80 motion mode cancel is active.");
			break;
		case 32:
			return("A G2 or G3 arc was commanded but there are no XYZ axis words in the selected plane to trace the arc.");
			break;
		case 33:
			return("The motion command has an invalid target. G2 , G3 , and G38.2 generates this error, if the arc is impossible to generate or if the probe target is the current position.");
			break;
		case 34:
			return("A G2 or G3 arc, traced with the radius definition, had a mathematical error when computing the arc geometry.Try either breaking up the arc into semi - circles or quadrants, or redefine them with the arc offset definition.");
			break;
		case 35:
			return("A G2 or G3 arc, traced with the offset definition, is missing the IJK offset word in the selected plane to trace the arc.");
			break;
		case 36:
			return("There are unused, leftover G-code words that aren't used by any command in the block.");
			break;
		case 37:
			return("The G43.1 dynamic tool length offset command cannot apply an offset to an axis other than its configured axis.The Grbl default axis is the Z - axis.");
			break;
		default:
			return("Unknown error code!");
			break;
		}
	}

	else if (is_alarm) {

		switch (code) {
		case 1:
			return("Hard limit triggered. Machine position is likely lost due to sudden and immediate halt. Re - homing is highly recommended.");
			break;
		case 2:
			return("G-code motion target exceeds machine travel. Machine position safely retained. Alarm may be unlocked.");
			break;
		case 3:
			return("Reset while in motion. Grbl cannot guarantee position. Lost steps are likely. Re-homing is highly recommended.");
			break;
		case 4:
			return("Probe fail. The probe is not in the expected initial state before starting probe cycle, where G38.2 and G38.3 is not triggered and G38.4 and G38.5 is triggered.");
			break;
		case 5:
			return("Probe fail. Probe did not contact the workpiece within the programmed travel for G38.2 and G38.4.");
			break;
		case 6:
			return("Homing fail. Reset during active homing cycle.");
			break;
		case 7:
			return("Homing fail. Safety door was opened during active homing cycle.");
			break;
		case 8:
			return("Homing fail. Cycle failed to clear limit switch when pulling off. Try increasing pull-off setting or check wiring.");
			break;
		case 9:
			return("Homing fail. Could not find limit switch within search distance. Defined as 1.5 * max_travel on search and 5 * pulloff on locate phases.");
			break;
		default:
			return("Unknown alarm code!");
			break;
		}

	}

	return "Invalid control path in OxCnc interpreter.";
}




/*

		NON-MEMBER FUNCTIONS RELATED TO THE GRBL MOVMEMENTS

*/


bool helperCenterWorm(AnthonysTimer &Ta, double time_limit_s, WormFinder& worms, OxCnc &Ox) {

	// Perform centering
	cout << "Centering target worm...\n";
	while (Ta.getElapsedTime() < time_limit_s*1.5 && worms.getTrackedCentroid().x > 0) {
		Ox.moveTowardTargetedPoint(worms.getTrackedCentroid(), worms.getPickupLocation(), worms.track_radius); /// Step worm towards center
		boost_interrupt_point();
		boost_sleep_ms(5);

		// If we think we've centered the worm, wait for a moment to confirm
		if (worms.isWormOnPickup()) {
			boost_sleep_ms(100);
			if (worms.isWormOnPickup()) {
				cout << "Worm ready to pick!" << endl;
				return true;
			}
		}
	}

	// If got here, the worm never centered
	return false;

}

//bool helperCenterWormCNN(AnthonysTimer &Ta, double time_limit_s, WormFinder& worms, OxCnc &Ox) {
//
//	// Perform centering
//	cout << "Centering target worm...\n";
//	while (Ta.getElapsedTime() < time_limit_s*1.5 && worms.getTrackedCentroid().x > 0) {
//		Ox.centerTargetedWorm(worms.getTrackedCentroid(), worms.getPickupLocation(), worms.track_radius); /// Step worm towards center
//		boost_interrupt_point();
//		boost_sleep_ms(5);
//
//		// If we think we've centered the worm, wait for a moment to confirm
//
//		if (worms.findWormNearestPickupLocCNN()) {
//			if (worms.isWormOnPickupCNN()) {
//				boost_sleep_ms(100);
//				if (worms.findWormNearestPickupLocCNN()) {
//					if (worms.isWormOnPickupCNN()) {
//						cout << "Worm ready to pick!" << endl;
//						return true;
//					}
//				}
//			}
//		}
//
//	}
//
//	// If got here, the worm never centered
//	return false;
//
//}

bool centerTrackedWormToPoint(WormFinder& worms, OxCnc& Ox, Point target_loc, int track_mode, bool wait_if_lost, bool use_mask_rcnn) {

	AnthonysTimer center_timer;
	AnthonysTimer extra_center_timer;
	int max_center_time = 6; // only try to center for at most this number of seconds
	int max_precision_center_time = 1;  // If we get the worm centered fast enough then we can spend a little time making the centering extra precise
	bool worm_centered = false;
	worms.worm_finding_mode = track_mode;
	bool remember_use_mask_rcnn = use_mask_rcnn;
	while (!worm_centered && center_timer.getElapsedTime() < max_center_time) {
		//cout << "attempting to center the tracked worm" << endl;
		// We need to track all the worms so we can reliably move to them in sequence with the high mag cam

		// Incrementally move the target point towards the tracked worm
		//cout << "Moving to worm at: " << worms.getTrackedCentroid().x << ", " << worms.getTrackedCentroid().y << endl;
		//cout << "Target location: " << target_loc.x << ", " << target_loc.y << endl;
		
		use_mask_rcnn = remember_use_mask_rcnn;
		
		if(use_mask_rcnn){
			// Run the mask rcnn network on the high mag image to get a precise location of the worm in the frame, then center based on this location
			maskRCNN_results results = maskRcnnRemoteInference(worms.img_high_mag_color);
			Mat mask = results.worms_mask;
			mask = mask == 1;
			
			// Check mask to ensure we got a worm, then get its contour
			int num_worms = results.num_worms;
			vector<Point> contour = isolateLargestContour(mask, 10);

			if ( num_worms == 0 || contour.size() == 0) {
				// We failed to find a contour on which to center, so we'll just try the normal low mag centering method
				use_mask_rcnn = false;
			}
			else {
				cout << "high mag color size: " << worms.img_high_mag_color.size() << endl;
				Point centroid = getCentroid(contour);
				Point high_mag_center = Point(worms.img_high_mag_color.cols / 2, worms.img_high_mag_color.rows / 2);
				cout << "worm centroid: " << centroid << endl;
				cout << "high mag cent: " << high_mag_center << endl;
				int center_within_dist = 70;
				
				/*Mat debug;
				mask.copyTo(debug);
				debug *= 50;
				cvtColor(debug, debug, COLOR_GRAY2RGB);
				drawCross(debug, centroid, colors::gg, 2, 8);
				drawCross(debug, high_mag_center, colors::rr, 2, 8);				
				circle(debug, high_mag_center, center_within_dist, colors::yy, 2);
				string winname = "High mag center targets";
				imshow(winname, debug);
				waitKey(0);
				destroyWindow(winname);*/

				worm_centered = worms.isWormCentered(high_mag_center, centroid, center_within_dist);
				cout << "Worm centered? : " << worm_centered << endl;
				if(!worm_centered)
					Ox.moveTowardTargetedPoint(centroid, high_mag_center, (double)worms.img_high_mag_color.cols, 0.45, 20);
	
			}
		}

		if (!use_mask_rcnn) {
			// Just center using low mag tracking mode to get centroid of the worm we're centering
			Ox.moveTowardTargetedPoint(worms.getTrackedCentroid(), target_loc, worms.track_radius);
			worm_centered = worms.isWormCentered(target_loc);
			extra_center_timer.setToZero();
			while (worm_centered && extra_center_timer.getElapsedTime() < max_precision_center_time && center_timer.getElapsedTime() < max_center_time) {
				// If we got the worm centered before the time limit then we can spend a little extra time making the centering very precise 
				if (extra_center_timer.getElapsedTime() < 0.01)
					cout << "Worm centered... spending a little extra time honing in on centering." << endl;
				Ox.moveTowardTargetedPoint(worms.getTrackedCentroid(), target_loc, worms.track_radius);
				boost_sleep_ms(10);
			}
			if (worm_centered)
				cout << "Done with precision centering. Extra time: " << extra_center_timer.getElapsedTime() << endl;

			if (worms.getTrackedWorm()->is_lost) {
				if (wait_if_lost) {
					cout << "Worm has been lost while centering. Waiting to see if we can re-find it." << endl;
					while (worms.getTrackedWorm()->is_lost && center_timer.getElapsedTime() < max_center_time) { boost_sleep_ms(200); }
				}
				else {
					cout << "Worm has been lost while centering. Stopping centering attempt." << endl;
					break;
				}
			}
		}

		//cout << "Worm is centered? " << worm_centered << endl;
		/*if (count >= 100) { break; }*/
		
		//count++;
		boost_sleep_ms(5);
	}
	if (!worm_centered) {
		cout << "Failed to center the worm to the target point." << endl;
		worms.worm_finding_mode = worms.WAIT;
		worms.all_worms.clear();
	}
	if (worms.getTrackedWorm()->is_lost) {
		cout << "Worm was lost while centering." << endl;
		worms.worm_finding_mode = worms.WAIT;
		worms.all_worms.clear();
	}

	//worms.worm_finding_mode = worms.WAIT;
	return worm_centered && !(worms.getTrackedWorm()->is_lost);

}

bool helperCenterWormCNNtrack(AnthonysTimer &Ta, double time_limit_s, WormFinder& worms, OxCnc &Ox) {

	// Perform centering
	cout << "Centering target worm...\n";
	// Ta.getElapsedTime() < time_limit_s*1.5 && worms.getTrackedCentroid().x > 0
	while (Ta.getElapsedTime() < time_limit_s*1.5) {

		Ox.moveTowardTargetedPoint(worms.getTrackedCentroid(), worms.getPickupLocation(), worms.track_radius); /// Step worm towards center
		boost_interrupt_point();
		boost_sleep_ms(5);

		// If we think we've centered the worm, wait for a moment to confirm

		if (worms.getWormNearestPickupLocCNN()) {
			if (worms.isWormCentered()) {
				boost_sleep_ms(50);
				if (worms.getWormNearestPickupLocCNN()) {
					if (worms.isWormCentered()) {
						cout << "Worm ready to pick!" << endl;
						return true;
					}
				}
			}
		}

	}

	// If got here, the worm never centered
	return false;

}


bool helperKeepWormCentered(AnthonysTimer &Ta, double time_limit_s, WormFinder& worms, OxCnc &Ox) {

	// Perform centering
	cout << "Centering target worm...\n";
	while (Ta.getElapsedTime() < time_limit_s*1.5 && worms.getTrackedCentroid().x > 0) {
		Ox.moveTowardTargetedPoint(worms.getTrackedCentroid(), worms.getPickupLocation(), worms.track_radius); /// Step worm towards center
		boost_interrupt_point();
		boost_sleep_ms(5);

		// If we think we've centered the worm, wait for a moment to confirm

		if (worms.getWormNearestPickupLocCNN()) {
			if (worms.isWormCentered()) {
				// boost_sleep_ms(100);
				if (worms.getWormNearestPickupLocCNN()) {
					if (worms.isWormCentered()) {
						//cout << "Worm ready to pick!" << endl;
					}
				}
			}
		}

	}

	return worms.isWormCentered();
}