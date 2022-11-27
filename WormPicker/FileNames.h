/*
	FileNames.h
	Anthony Fouad
	Fang-Yen Lab
	Fall 2018

	Defines the class thatcan be used to store all the myraid file names used to store WormWatcher/Picky parameters
*/
#pragma once
#ifndef FILENAMES_H
#define FILENAMES_H

#include "AnthonysCalculations.h"
#include <string>

class FileNames {

public:

	// Constructor, pparams must be supplied
	FileNames(std::string ppp, std::string plgs) : pparams(ppp), plogs(plgs) {

		// File names for GRBL parameters
		grbl_params = pparams + "params_GrblW.txt";
		grbl_stored_pos = pparams + "params_GrblW_StoredPos.txt";

		// file name for experiment parameters (timing sequence and light length)
		acquisition = pparams + "params_Acquisition.txt";

		// file names for plate tray parameters (where located and which ones to do)
		camera_params = pparams + "params_Camera.txt";
		user_params = pparams + "params_PlateTray_User.txt";
		plate_names = pparams + "params_PlateTray_PlateNames.txt";
		plate_positions = pparams + "params_PlateTray_PlatePos.txt";
		plate_script = pparams + "params_PlateTray_ActionScript.txt";
		current_state = pparams + "params_PlateTray_CurrentState.txt";
		calibration_samples = pparams + "params_PlateTray_CalibrationSamples.txt";
		calibration_program = pparams + "CNC_calibrate_rectangular_grid_v02.exe";

		// file name for debug events log and emailing
		debug_log = plogs + "debug_log_" + getCurrentDateString() + ".txt";
		email_program = pparams + "WormWatcherEmailNotify.exe";
		email_alert = pparams + "Email_Alert_Current.txt";
		email_settings = pparams + "Email_Alert_Settings.txt";

		// file name for WWAnalyzer
		wwa_params = pparams + "WWAnalyzer\\WWAnalyzerAutoRunSettings.txt";
		wwa_program = pparams + "WWAnalyzer\\WWAnalyzer.exe";
		wwa_roi_files_dir = pparams + "WWAnalyzer\\ROI_Reference\\";

		// File names for Worm picking-specific parameters
		dynamixel_pick = pparams	+ "params_DynamixelPick.txt";
		pickup = pparams			+ "params_pickup.txt";
		grbl1_params = pparams		+ "params_OxCnc.txt";
		grbl1_stored_pos = pparams	+ "params_OxCnc_StoredPos.txt";
		grbl2_params = pparams		+ "params_Grbl2.txt";
		sorted_blobs = pparams		+ "params_SortedBlobs.txt";
		thresh = pparams			+ "params_thresh.txt";
		use_components = pparams	+ "params_Use_Components.txt";

		//json
		cameras_json = pparams		+ "cameras.json";
	}

	std::string pparams;
	std::string plogs;
	std::string grbl_params;			/// WormWatcher grbl file
	std::string grbl_stored_pos;
	std::string acquisition;
	std::string camera_params;
	std::string user_params;
	std::string plate_names;
	std::string plate_positions;
	std::string plate_script;
	std::string current_state;
	std::string calibration_samples;
	std::string calibration_program;
	std::string debug_log;
	std::string email_program;
	std::string email_alert;
	std::string email_settings;
	std::string wwa_params;
	std::string wwa_program;
	std::string wwa_roi_files_dir;
	std::string cameras_json;

	// Additional settings files for WormPicker
	std::string dynamixel_pick;
	std::string pickup;
	std::string grbl1_params;
	std::string grbl1_stored_pos;
	std::string grbl2_params;
	std::string sorted_blobs;
	std::string thresh;
	std::string use_components;
};


#endif