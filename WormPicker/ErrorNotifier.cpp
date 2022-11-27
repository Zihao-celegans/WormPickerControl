/*
	ErrorNotifier.cpp
	Anthony Fouad
	Fang-Yen Lab
	January 2019

	Notify the user of errors per instructions in Email_Alert_Settings
*/

#include "ErrorNotifier.h"
#include <string>
#include <vector>
#include <fstream>
#include "ControlledExit.h"
#include "AnthonysCalculations.h"
#include "ThreadFuncs.h"
#include "Config.h"
#include "boost/algorithm/string.hpp"



using json = nlohmann::json;
using namespace std;
using namespace boost;


// Constructor
ErrorNotifier::ErrorNotifier(ofstream& dl) : 
	debug_log(dl) 
{

	json errors_config = Config::getInstance().getConfigTree("errors");

	system_name = errors_config["system name"].get<string>();

	// add each error type in the JSON file
	for (json error : errors_config["alerts"]) {
		addErrorType(error);
	}
	
	first_email = true;

	// Non-customizable general purpose actions
	errors.push_back(ErrorOption(LOG_ONLY, 0, 0, 0));
	errors.push_back(ErrorOption(PRINT_ONLY, 1, 0, 0));
	errors.push_back(ErrorOption(PRINT_EMAIL_ONLY, 1, 1, 0));
	errors.push_back(ErrorOption(PRINT_EMAIL_ABORT, 1, 1, 1));
	errors.push_back(ErrorOption(PRINT_ABORT_ONLY, 1, 0, 1));

	return;
}

bool ErrorNotifier::addErrorType(json error) {
	string name = error["name"].get<string>();
	ErrorTypes type;
	bool p, e, a;
	// parse the name
	if (name == "LABJACK_DISCONNECTED") {
		type = LABJACK_DISCONNECTED;
	}
	else if (name == "CAM_PERMANENTLY_DISCONNECTED") {
		type = CAM_PERMANENTLY_DISCONNECTED;
	}
	else if (name == "NO_RED_DETECTED") {
		type = NO_RED_DETECTED;
	}
	else if (name == "NO_BLUE_DETECTED") {
		type = NO_BLUE_DETECTED;
	}
	else if (name == "PLATE_NOT_RECOGNIZED") {
		type = PLATE_NOT_RECOGNIZED;
	}
	else if (name == "EXPERIMENT_SUCCESS") {
		type = EXPERIMENT_SUCCESS;
	}
	else if (name == "QRCODE_NOT_FOUND") {
		type = QRCODE_NOT_FOUND;
	}
	else {
		cout << "Unrecognized error name, ignoring" << endl;
		return false;
	}

	// parse the alerts
	p = error["print to cmd"].get<bool>();
	e = error["email alert"].get<bool>();
	a = error["abort program"].get<bool>();
	errors.push_back(ErrorOption(type, p, e, a));
	return true;
}

// Notify: Replacement for printTimedError
void ErrorNotifier::notify(int error_type, string msg) {

	// Find the error options corresponding to the error type. If none is found, use default EMAIL ONLY
	ErrorOption this_error(PRINT_EMAIL_ONLY, 1, 1, 0);
	for (int i = 0; i < errors.size(); i++) {
		if (error_type == errors[i].error_name) {
			this_error = errors[i];
		}
	}

	// Prepend timestamp to the message
	msg = getCurrentTimeString(false) + "    " + msg;

	// Store the last 50 messages if needed
	/*error_history.push_back(msg);
	if (error_history.size() > 50)
		error_history.pop_front();*/ // not needed for now and risk of thread non-safety

	// Print the message to the command line if requested
	if (this_error.print)
		cout << msg << endl;

	// Print to debug file REGARDLESS of user options
	debug_log << msg << endl;
	debug_log.flush();

	// If requested, email the user (depending on the settings in the executable, might not email every time)
	if (this_error.email && (T_last_email.getElapsedTime() > 10*60 || first_email)) {

		// Prevent sending too many emails - wait at least 5s, UNLESS this is the success email, that doesn't reset
		if (!contains(msg, "WormWatcher plate cycle completed")) {
			T_last_email.setToZero();
			first_email = false;
		}
			

		// Print error to the email alert file (overwriting existing)
		ofstream outfile(filenames::email_alert);
		outfile << "SYSTEM: " << system_name << endl;
		outfile << msg << endl << endl;
		outfile << "NOTE: No further error emails of any type will be sent for at least 10 minutes. If an error has occurred, please check the system as soon as possible.";
		outfile.close();
		boost_sleep_ms(600);

		// Run email engine
		string command = filenames::email_program;
		//string result = executeExternal(command.c_str());
		std::system(command.c_str());
	}

	// If requested, abort program execution
	if (this_error.abort) {
		controlledExit(0,"");
	}
}

void ErrorNotifier::notify(errorstruct error) {
	notify(error.handle, error.to_string());
}

bool ErrorNotifier::scanHistory(string searchstr) {

	bool contains_it = false;

	for (int i = 0; i < error_history.size(); i++) {
		if (contains(error_history[i], searchstr)) {
			contains_it = true;
			break;
		}
	}

	return contains_it;
}


/*
	Define the error struct
*/

errorstruct::errorstruct() {
	code = 0;
	message = "undefined error";
	handle = PRINT_ONLY;
}


errorstruct::errorstruct(int _code, std::string _msg, ErrorTypes _handle) {
	code = _code;
	message = _msg;
	handle = _handle;
}


std::string errorstruct::to_string() {
	std::string s = std::to_string(code) + ": " + message;

	// informational
	if (code >= 500) {
		return "WW LOG " + s;
	}

	// warning
	if (code >= 400) {
		return "WW WARNING " + s;
	}

	// error
	return "WW ERROR " + s;
}



/*
	Define the namespace

*/


// Successful completion (000)
errorstruct errors::success = errorstruct(000, 
	"WormWatcher plate cycle completed.\nNOTE: successful completion notifications will not be sent more often than one per 8 hours.",
	EXPERIMENT_SUCCESS);


// hardware faults (100)
errorstruct errors::homing_error = errorstruct(100, "failed to home CNC", GRBL_PERMANENTLY_DISCONNECTED);
errorstruct errors::camera_disconnect = errorstruct(101, "failed to connect to camera", CAM_PERMANENTLY_DISCONNECTED);
errorstruct errors::grbl_disconnect = errorstruct(102, "failed to connect to GRBL", GRBL_PERMANENTLY_DISCONNECTED);
errorstruct errors::cnc_move_fail = errorstruct(103, "failed to move the CNC after many attemps", GRBL_PERMANENTLY_DISCONNECTED);

// experiment failures (200)
errorstruct errors::not_enough_images = errorstruct(200, "failed to analyze session data, not enough images", PRINT_ONLY);
errorstruct errors::invalid_img_size = errorstruct(201, "failed to analyze session data, images are of different sizes", PRINT_ONLY);
errorstruct errors::save_data = errorstruct(202, "failed to save CSV", ANALYSIS_ERROR);
errorstruct errors::well_segment_fail = errorstruct(203, "Failed to recognize wells in session data", PRINT_ONLY);
errorstruct errors::plate_not_recognized = errorstruct(204, "Plate could not be recognized in the image", PLATE_NOT_RECOGNIZED);
errorstruct errors::before_seq_fail = errorstruct(205, "Failed to acquire BEFORE sequence - retrying", PRINT_ONLY);
errorstruct errors::save_img_fail = errorstruct(206, "Failed to save the image", PRINT_EMAIL_ABORT);
errorstruct errors::session_name_fail = errorstruct(207, "Error confirming session name - no images saved?", PRINT_ONLY);
errorstruct errors::light_disabled = errorstruct(208, "Error toggling the light. Light is disabled in config", LOG_ONLY);
errorstruct errors::red_failsafe = errorstruct(209, "Red light ended by failsafe timer", PRINT_EMAIL_ONLY); // 15 min of red light timing data will now be printed to the debug log
errorstruct errors::no_red_detected = errorstruct(210, "Red light not detected by camera", NO_RED_DETECTED);
errorstruct errors::no_blue_detected = errorstruct(211, "Blue light not detected by camera", NO_BLUE_DETECTED);
errorstruct errors::lj_toggle_warning = errorstruct(212, "Labjack may be unable to toggle lights", LABJACK_DISCONNECTED);
errorstruct errors::invalid_path = errorstruct(213, "Failed to parse invalid path string", PRINT_ONLY);
errorstruct errors::low_space = errorstruct(214, "Low disk space - please replace", PRINT_EMAIL_ONLY);
errorstruct errors::low_space_critical = errorstruct(214, "Low disk space - experiment aborted", PRINT_EMAIL_ABORT);

// invalid parameters (300)
errorstruct errors::invalid_param_file = errorstruct(300, "error reading config file", PRINT_ABORT_ONLY);
errorstruct errors::invalid_script_state = errorstruct(301, "State loaded refers to an invalid step. Script will reset", PRINT_ONLY);
errorstruct errors::invalid_script = errorstruct(302, "Script invalid or not found", PRINT_ABORT_ONLY);
errorstruct errors::invalid_script_step = errorstruct(303, "Invalid script step encountered", PRINT_ONLY);
errorstruct errors::invalid_script_step_id = errorstruct(304, "Invalid ID for current script step", PRINT_ABORT_ONLY);

// warnings (400)
errorstruct errors::cnc_move_attempt_fail = errorstruct(400, "failed to move CNC, trying again", PRINT_ONLY);
errorstruct errors::coord_oob = errorstruct(401, "aborting movement, target coordinated out of bounds", PRINT_ONLY);
errorstruct errors::lid_lift_fail = errorstruct(402, "failed to lift lid because one is already in the air", PRINT_ONLY);
errorstruct errors::homing_attempt_fail = errorstruct(403, "failed to home the CNC, trying again", PRINT_ONLY);
errorstruct errors::lid_lower_fail = errorstruct(404, "failed to lower lid because no lid is in the air", PRINT_EMAIL_ONLY);
errorstruct errors::failed_grbl_read = errorstruct(405, "failed to read from GRBL, trying again", PRINT_ONLY);
errorstruct errors::failed_grbl_repsonse = errorstruct(406, "Grbl failed to respond. Attempting to restart", PRINT_ONLY);
errorstruct errors::position_mismatch = errorstruct(407, "Position mismatch, acutal position does not match expected. checking again...", PRINT_ONLY);
errorstruct errors::record_interval = errorstruct(408, "Record intervals other than 5s may be untested", PRINT_ONLY);
errorstruct errors::save_img_slow = errorstruct(409, "Frame is not finished saving, indicating slow save times", PRINT_ONLY);

// informational (500)
errorstruct errors::camera_reconnect_attempt = errorstruct(500, "attempting to reconnect to camera", PRINT_ONLY);
errorstruct errors::waiting_for_homing = errorstruct(501, "waiting for homing", PRINT_ONLY);
errorstruct errors::homing_skip = errorstruct(502, "skipping homing", PRINT_ONLY);
errorstruct errors::homing_completed = errorstruct(503, "homing completed", PRINT_ONLY);
errorstruct errors::cap_power_cycle = errorstruct(504, "Power cycling the pick's capacitive sensor", PRINT_ONLY);
errorstruct errors::rehoming = errorstruct(505, "Re-homing the CNC", PRINT_ONLY);



