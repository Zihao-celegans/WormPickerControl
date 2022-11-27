/*
	ErrorNotifier.h
	Anthony Fouad
	Fang-Yen Lab
	January 2019

	Notify the user of errors per instructions in Email_Alert_Settings
*/

#pragma once
#ifndef ERRORNOTIFIER_H
#define ERRORNOTIFIER_H

#include <string>
#include <vector>
#include <deque>
#include <iostream>
#include <fstream>
#include <map>
#include "json.hpp"
#include "AnthonysTimer.h"


// List of errors 
enum ErrorTypes {

	EXPERIMENT_SUCCESS = 5,
	PRINT_ABORT_ONLY = 4,			/// Print and abort but don't send an email
	LOG_ONLY = 3,					/// Don't show error anywhere but write it in the log
	PRINT_ONLY = 2,					/// Print, log
	PRINT_EMAIL_ONLY = 1,			/// Print, log and email
	PRINT_EMAIL_ABORT = 0,			/// Print, log, email and abort the program
	NO_BLUE_DETECTED = -1,
	NO_RED_DETECTED = -2,
	LABJACK_DISCONNECTED = -3,
	CAM_PERMANENTLY_DISCONNECTED = -4,
	GRBL_PERMANENTLY_DISCONNECTED = -5,
	PLATE_NOT_RECOGNIZED = -6,
	ANALYSIS_ERROR = -7,
	QRCODE_NOT_FOUND = -8
};



class ErrorOption {

public:

	// Constructor
	ErrorOption(int en, bool pt, bool em, bool ab) : error_name(en), print(pt), email(em), abort(ab){}

	int error_name;
	bool print;
	bool email;
	bool abort;

};

class errorstruct {
public:
	int code;
	std::string message;
	ErrorTypes handle;

	errorstruct();
	errorstruct(int _code, std::string _msg, ErrorTypes _handle);
	std::string to_string();
};


namespace errors {

	// Successful completion (000)
	extern errorstruct success;

	// hardware faults (100)
	extern errorstruct homing_error;
	extern errorstruct camera_disconnect;
	extern errorstruct grbl_disconnect;
	extern errorstruct cnc_move_fail;

	// experiment failures (200)
	extern errorstruct not_enough_images;
	extern errorstruct invalid_img_size;
	extern errorstruct save_data;
	extern errorstruct well_segment_fail;
	extern errorstruct plate_not_recognized;
	extern errorstruct before_seq_fail;
	extern errorstruct save_img_fail;
	extern errorstruct session_name_fail;
	extern errorstruct light_disabled;
	extern errorstruct red_failsafe;
	extern errorstruct no_red_detected;
	extern errorstruct no_blue_detected;
	extern errorstruct lj_toggle_warning;
	extern errorstruct invalid_path;
	extern errorstruct low_space;
	extern errorstruct low_space_critical;

	// invalid parameters (300)
	extern errorstruct invalid_param_file;
	extern errorstruct invalid_script_state;
	extern errorstruct invalid_script;
	extern errorstruct invalid_script_step;
	extern errorstruct invalid_script_step_id;

	// warnings (400)
	extern errorstruct cnc_move_attempt_fail;
	extern errorstruct coord_oob;
	extern errorstruct lid_lift_fail;
	extern errorstruct homing_attempt_fail;
	extern errorstruct lid_lower_fail;
	extern errorstruct failed_grbl_read;
	extern errorstruct failed_grbl_repsonse;
	extern errorstruct position_mismatch;
	extern errorstruct record_interval;
	extern errorstruct save_img_slow;

	// informational (500)
	extern errorstruct camera_reconnect_attempt;
	extern errorstruct waiting_for_homing;
	extern errorstruct homing_skip;
	extern errorstruct homing_completed;
	extern errorstruct cap_power_cycle;
	extern errorstruct rehoming;
}

class ErrorNotifier {

public:
	// Constructor - reads preferences for all error types
	ErrorNotifier(std::ofstream& dl);

	// Print error according to settings. Use 
	void notify(int error_type, std::string msg);
	void notify(errorstruct error);

	// Check if the last-50 errors history contains a string anywhere
	bool scanHistory(std::string searchstr);

protected:
	// Add a new error type to the list
	bool addErrorType(nlohmann::json error_config);

	std::vector<ErrorOption> errors;
	std::deque<std::string> error_history;
	std::string system_name;
	std::ofstream& debug_log;
	AnthonysTimer T_last_email;
	bool first_email;

};


#endif