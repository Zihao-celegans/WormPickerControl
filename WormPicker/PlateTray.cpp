/*
	PlateTray.cpp
	Anthony Fouad
	Fang-Yen Lab
	Fall 2018

	Class that governs the names, locations, and other parameters of plates on the CNC robot.
*/

#pragma once
// Anthony's includes
#include "PlateTray.h"
#include "PlateTrayScriptStep.h"
#include "AnthonysCalculations.h"
#include "AnthonysKeyCodes.h"
#include "AnthonysColors.h"
#include "ImageFuncs.h"
#include "ControlledExit.h"
#include "ScriptActions.h"
#include "Config.h"

// OpenCV includes
#include "opencv2\opencv.hpp"

// JSON include
#include "json.hpp"

// Standard includes
#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>
#include <string>
#include <algorithm>

// Boost includes
#include "boost/assert.hpp"
#include "boost/algorithm/string.hpp"


// Socket programming includes
#include "SocketProgramming.h"

// Namespaces
using namespace std;
using namespace cv;
//using namespace boost::numeric::ublas; // conflicts with std?
using json = nlohmann::json;

// Constructor - load parameters - also loaded prior to running
PlateTray::PlateTray(std::ofstream &dl, ErrorNotifier &ee, std::vector<std::shared_ptr<std::string>>& sps,
					std::vector<int> cns, bool &dm, string& _cpt, bool _isww) :
	debug_log(dl),
	full_path_curr(sps),
	cam_nums(cns),
	dummy_mode(dm),
	Err(ee),
	plateMat(Config::getInstance().getConfigTree("plates")["num rows"].get<int>(), 
	Config::getInstance().getConfigTree("plates")["num cols"].get<int>()),
	current_plate_type(_cpt),
	is_wormwatcher(_isww)
{	
	// Load all parameters
	loadParams();

	// Load current plate from disk
	loadPlateState();

}


PlateTray::PlateTray(const PlateTray& Tr) :
	debug_log(Tr.debug_log),
	full_path_curr(Tr.full_path_curr),
	cam_nums(Tr.cam_nums),
	dummy_mode(Tr.dummy_mode),
	Err(Tr.Err),
	plateMat(Config::getInstance().getConfigTree("plates")["num rows"].get<int>(),
	Config::getInstance().getConfigTree("plates")["num cols"].get<int>()),
	current_plate_type(Tr.current_plate_type),
	is_wormwatcher(Tr.is_wormwatcher)
{
	n_rows = Tr.n_rows;
	n_cols = Tr.n_cols;
	expt_hours = Tr.expt_hours;
	all_plate_parent_paths = Tr.all_plate_parent_paths;
	all_plate_ref_files = Tr.all_plate_ref_files;
	plateMat = Tr.plateMat;
	calibration_coords = Tr.calibration_coords;
	calibration_ids = Tr.calibration_ids;
	script = Tr.script;
	script_step = Tr.script_step;
	first_valid_script_step = Tr.first_valid_script_step;
	plate_script_was_running = Tr.plate_script_was_running;
	plate_script_is_running = Tr.plate_script_is_running;
	plate_action_is_running = Tr.plate_action_is_running;
	plate_curr = Tr.plate_curr;
	name_curr = Tr.name_curr;
	pos_curr = Tr.pos_curr;
	current_plate_type = Tr.current_plate_type;

}
bool PlateTray::loadParams() {
	
	// Prevent loading if the script is already running (especially on user presses)
	if (plate_script_is_running) {
		cout << "FAILED to load parameters because plate script is running." << endl;
		return false;
	}

	cout << "Loading: User parameters... ";
	debug_log << getCurrentTimeString() << " " << "Loading: User parameters... ";

	// add each num cols in the list
	json plates_config = Config::getInstance().getConfigTree("plates");
	n_rows = plates_config["num rows"].get<int>();
	n_cols = plates_config["num cols"].get<int>();

	// Get whether listen to Python for script execution 
	listen_to_Socket = Config::getInstance().getConfigTree("API scripting")["Listen to Socket"].get<bool>();

	if (listen_to_Socket) {
		json execution_config = Config::getInstance().getConfigTree("API scripting")["script_execution"];
		raw_ip_address = execution_config["raw_ip_address"].get<string>();
		port_num = execution_config["port_num"].get<unsigned short>();
	}

	// read the experiment hours
	expt_hours.clear();
	for (auto iter : plates_config["experiment hrs"]) {
		expt_hours.push_back(iter.get<int>());
	}
	sort(expt_hours.begin(), expt_hours.end());

	// verify parameters
	controlledExit(expt_hours.size() > 0, "Failure to read PlateTray user parameters -- expt_hours has no data");
	// controlledExit(n_cols.size() == n_rows, "Failure to read PlateTray user parameters -- n_cols.size() does not match n_rows");

	cout << "Names... ";
	debug_log << "Names... ";

	// Validate User parameters
	controlledExit(expt_hours.size() > 0, "Failure to read PlateTray user parameters -- expt_hours has no data");
	// controlledExit(n_cols.size() == n_rows, "Failure to read PlateTray user parameters -- n_cols.size() does not match n_rows");

	all_plate_parent_paths.clear();

	for (auto plate_json : plates_config["plates"]) {
		int r, c;
		double x, y, z;
		string name, path, plate_type;

		// Load information about this plate from the JSON
		r = plate_json["row"].get<int>();
		c = plate_json["col"].get<int>();
		name = plate_json["name"].get<string>();
		path = plate_json["img path"].get<string>();
		x = plate_json["position"]["x"].get<double>();
		y = plate_json["position"]["y"].get<double>();
		z = plate_json["position"]["z"].get<double>();
		plate_type = plate_json["type"].get<string>();

		// Add that information to the plate structure map
		plate newPlate;
		newPlate.coord = Point3d(x, y, z);
		newPlate.name = name;
		newPlate.fullPath = path;
		newPlate.type = plate_type;

		plateMat(r, c) = newPlate;

		// make sure the file path in the list is valid, unless it is "NONE"
		if (!path.empty()) {
			if (!makePath(path) && !path.empty()) {
				cout << "Path: " << path << "is invalid. Program will now exit..." << endl;
				debug_log << getCurrentTimeString() << " " << "Path: " << path << "is invalid.\n"
					<< "Press ENTER to exit...\n" << endl;
				controlledExit(0, "");
			}
		}
		

		// Store parent path if we haven't already
		if (!vsContains(all_plate_parent_paths, path) && path.compare("NONE") != 0 && name.compare("NONE") != 0) {
			all_plate_parent_paths.push_back(path);
		}
	}

	//controlledExit(r == n_rows - 1 && c == n_cols[n_cols.size() - 1] - 1,
	//	"Failure to read PlateTray coordinates -- size does not match specifications");

	// Load the script
	cout << "Script... ";
	debug_log << "Script... ";
    if (!setupPlateScript()) { return false; }
	cout << "Completed!\n";
	debug_log << "Completed!" << endl;

	// Find the first valid script step (when homing will occur)
	findFirstValidScriptStep();

	return true;
}


bool PlateTray::setupPlateScript() {

	if (is_wormwatcher)
		autoGeneratePlateScript();

	else
		loadPlateScript();


	// Enforce non-empty script
	controlledExit(script.size() > 0, "Script is empty");
	

	//If script.size() > 0, indicate the plate script has been successfully setup
	return (script.size() > 0);
}

void PlateTray::loadPlateScript() {

	/*
		Depending on whether receiving commands from a socket,
		the script object is configured in different ways.

		If NOT receiving commands from socket (listen_to_socket = false), simply from inside the C++,
		the script is constructed via reading from a local txt file edited by the user.

		If receiving commands from socket (listen_to_socket = true), from another language e.g. Python,
		the script is constructed from the string received from the socket
	*/

	// If NOT receiving commands from socket
	if (!listen_to_Socket) {
		// Clear existing script
		script.clear();

		// Setup script file
		ifstream inputfile(filenames::plate_script);
		string this_step_name, this_step_action;
		int r, c;
		vector<string> args;

    string inputLine;
    vector<string> tokens;
		std::string delimiter = "\t";

		// Load all valid rows and format as script steps
		while (getline(inputfile, inputLine))
		{
			args.clear();

      boost::split(tokens, inputLine, boost::is_any_of(delimiter));
      if (tokens.size() >= 4) {
          this_step_name = tokens[0];
          r = stoi(tokens[1]);
          c = stoi(tokens[2]);
          this_step_action = tokens[3];
          for (int j = 4; j < tokens.size(); j++) {
              args.push_back(tokens[j]);
          }
      } else break;

			// make sure the script step specifies a valid index
			if (!validateStepInfo(MatIndex(r, c))) {
				Err.notify(errors::invalid_script);
				break;
			}

			// Add the ScriptStep to the Script
			script.push_back(PlateTrayScriptStep(this_step_name, r, c, this_step_action, args));
		}

		inputfile.close();
	}

	// If receiving commands from socket
	else if(listen_to_Socket) {

		// Construct the script according to some pre-specified format

		// Clear existing script
		script.clear();

		// Setup variables
		string this_step_name, this_step_action;
		int r, c;
    vector<string> args;

		// Split the string by default delimiter "\t"
		// Adapted from https://stackoverflow.com/questions/14265581/parse-split-a-string-in-c-using-string-delimiter-standard-c

		std::string delimiter = "\t";

		size_t pos = 0;
		string token;
		vector<string> tokens;
		string Msg(Message_from_socket);

		while ((pos = Msg.find(delimiter)) != std::string::npos) {
			token = Msg.substr(0, pos);
			//std::cout << token << std::endl;
			tokens.push_back(token);
			Msg.erase(0, pos + delimiter.length());
		}
		tokens.push_back(Msg); // Append the last token object

		// Make sure the script step string has at least 4 entries before parsing
		if (tokens.size() < 4) {
			Err.notify(errors::invalid_script);
			return;
		}

    this_step_name = tokens[0];
    r = stoi(tokens[1]);
    c = stoi(tokens[2]);
		this_step_action = tokens[3];
    for (int i = 4; i < tokens.size(); i++) {
      args.push_back(tokens[i]);
    }

    // make sure the script step specifies a valid index
    if (!validateStepInfo(MatIndex(r, c))) {
      Err.notify(errors::invalid_script_step_id);
      return;
    }

    // Add the ScriptStep to the Script
    script.push_back(PlateTrayScriptStep(this_step_name, r, c, this_step_action, args));
	}
}

void PlateTray::autoGeneratePlateScript() {

	// Clear any existing script fragments
	script.clear();
	
	for (int r = 0; r < n_rows; r++) {
		for (int c = 0; c < n_cols; c++) {
			script.push_back(PlateTrayScriptStep("action", r, c, "WWEXP", {}));
		}
	}
}

void PlateTray::loadPlateState() {

	json state = Config::getInstance().getConfigTree("state");
	plate_script_is_running = false;

	// Load current running state. Only load the script step if we are resuming from where we left off
	plate_script_was_running = state["was running"].get<bool>();
	if (plate_script_was_running)
		script_step = state["script step"].get<int>();


	// If current state loaded from disk is out of bounds, reset the script to step 0
	if (script_step < 0 || script_step > script.size() - 1) {
		Err.notify(errors::invalid_script_state);
		resetPlateSeries();
	}

	updateStepInfo();
}

void PlateTray::savePlateState() {
	
	// Load the JSON fresh to grab new changes before overwriting
	Config::getInstance().loadFiles();

	// Overwrite entries in the JSON object
	json state = Config::getInstance().getConfigTree("state");
	state["was running"] = plate_script_is_running;
	state["script step"] = script_step;
	state["plate"]["row"] = script[script_step].id.row;
	state["plate"]["col"] = script[script_step].id.col;
	state["position"]["x"] = pos_curr.x;
	state["position"]["y"] = pos_curr.y;
	state["position"]["z"] = pos_curr.z;
	state["plate name"] = name_curr;
	state["path"] = *full_path_curr[0];
	
	// Replace the tree and write
	Config::getInstance().replaceTree("state", state);
	Config::getInstance().writeConfig();

}



bool PlateTray::startPlateSeries() {

	/// re-load the plate names and script from disk in case there have been additions or changes
	if (!loadParams()) return false;

	/// Load the current state from disk
	loadPlateState();

	/// is_running is true regardless of the previous state loaded from disk
	plate_script_is_running = true;
	plate_script_was_running = false;

	/// Check that a valid plate exists in the current location, if not, find next valid plate
	checkCurrentPlate();

	/// Save the new (running) state to disk
	savePlateState();

	return true;

}


bool PlateTray::checkForStartTime() {

	// Exit with true if already running, nothing to do here...
	if (plate_script_is_running)
		return true;


	// Continuous running: any start hour is negative
	bool is_negative = false;
	for (int i = 0; i < expt_hours.size(); i++) {
		if (expt_hours[i] < 0)
			is_negative = true;
	}
		

	// Get current hour
	double hr = getCurrentHour();
	double min = getCurrentMinute();
	double sec = getCurrentSecond();


	// Start if the current hour is in the start vector AND the current minute is 0 AND current second is less than 20 (head of the hour) 
	///		This helps prevent double starts if the sequence finishes within the same hour as starting.
	///		ALTERNATIVELY: Supply negative start time(s) to signal continuous running
	bool is_start = ((find(expt_hours.begin(), expt_hours.end(), hr) != expt_hours.end()) && min == 0 && sec < 20) ||
					is_negative;

	// If starting is requested...
	if (is_start) 
		startPlateSeries();

	return is_start;
}



bool PlateTray::checkIfPlateSeriesIsRunning() {
	return plate_script_is_running;
}

void PlateTray::setPlateSeriesRunning() {
	plate_script_is_running = true;
	plate_script_was_running = false;
}

void PlateTray::stopPlateSeriesRunning() {
	plate_script_is_running = false;
	plate_script_was_running = true;
}

void PlateTray::resetPlateSeries() {
	plate_script_is_running = false;
	plate_script_was_running = false;
	script_step = 999999999; /// OOB script step will trigger resetting to zero or the first valid step
	setNextPlate(); /// Apply the reset immediately

}


/* 

	set next plate for picking (or other action), based on the user - defined script.

*/

bool PlateTray::setNextPlate() {

	// Temporarily set current name to NONE. We will try to fill it in with a valid plate
	name_curr = "";
	
	// Find next step at which a valid plate is present. Locations with no valid plates are loaded in with blank names nad paths
	int n_tries = 0, max_tries = 99999;
	while (!name_curr.size() && ++n_tries < max_tries) {

		// Increment script step. Have we finished all rows? If so, reset to the first step and mark for completion
		if (++script_step > script.size() - 1) {
			plate_script_is_running = false;		/// Set to false to indicate finished. 
			script_step = 0;						/// Go back to step 0.
		}

		// Retrieve plate info corresponding to this step
		updateStepInfo();

		// If no plate is present here, note that the step should be skipped

	}
	if (n_tries >= max_tries) {
		cout << "NO VALID PLATES WERE FOUND ON THE TRAY" << endl;
		debug_log << getCurrentTimeString() << " " << "NO VALID PLATES WERE FOUND ON THE TRAY" << endl;
	}
		

	// Store current action, position, and other details on disk
	savePlateState();

	// Indicate whether a valid next plate was found (otherwise we restarted or are on the first step)
	return script_step > 0;
}


// Check that the current plate is valid (decrement step and then setNextPlate() )
bool PlateTray::checkCurrentPlate() {
	script_step--;
	return setNextPlate();
}


void PlateTray::updateStepInfo() {

	// Set next plate location
	MatIndex plate_curr_bak = plate_curr;
	plate_curr = script[script_step].id;

	// If plate_curr is a valid plate index, proceed with assigning its details
	/// Note:	Because validation is also performed when loading disk files, the 
	///			requested plate should always be valid. This is a double-check.
	/// Note2:	If a plate starts imaging terminate to midnight but imaging passes midnight,
	///			the data will continue to be saved in the same day's path 
	///			as long as full_path_curr is not updated anywhere else. 

	if (validateStepInfo(plate_curr)) {
		pos_curr = plateMat(plate_curr.row, plate_curr.col)->coord;
		name_curr = plateMat(plate_curr.row, plate_curr.col)->name;

		// Change script arguments



		updateCurrentPath();
		current_plate_type = plateMat(plate_curr.row, plate_curr.col)->type;
	}

	// If not, treat it as an empty well so that actions will not be attempted at this invalid location
	else {
		plate_curr = plate_curr_bak;
		/// Old position coordinates are not changed - don't move anywhere
		name_curr = "";
		for (int i = 0; i<full_path_curr.size(); i++)
			*full_path_curr[i] = "";
	}
}

// Update the current path, a vector of pointers that are shared with the main function
void PlateTray::updateCurrentPath() {
	if (full_path_curr.size() == 1) {
		*full_path_curr[0] = plateMat(plate_curr.row, plate_curr.col)->fullPath + "\\" + plateMat(plate_curr.row, plate_curr.col)->name + "\\" + getCurrentTimeString() + "\\" ;
	}
	else {
		for (int i = 0; i < full_path_curr.size(); i++) {
			*full_path_curr[i] = plateMat(plate_curr.row, plate_curr.col)->fullPath + "\\" + plateMat(plate_curr.row, plate_curr.col)->name + "\\cam" + to_string(cam_nums[i]) + "\\" + getCurrentTimeString() + "\\";
		}
	}
}



// Return true if the requested plate index exists on the PlateTray
bool PlateTray::validateStepInfo(MatIndex test_plate) {

	bool row_exists = test_plate.row >= 0 && test_plate.row <= n_rows-1;
	bool col_exists = test_plate.col >= 0 && test_plate.col <= n_cols-1;
	
	//cout << "row and col exist? " << (row_exists && col_exists) << endl;

	return row_exists && col_exists;

}

// Find the first step at which a plate is present and can be used
void PlateTray::findFirstValidScriptStep() {

	// Set next plate location
	first_valid_script_step = 0;
	for (int i = 0; i < script.size(); i++) {

		MatIndex temp = script[i].id;

		// Exit when we find the first valid plate (name isn't empty)
		if (validateStepInfo(temp)) {

			string temp_name = plateMat(temp.row, temp.col)->name;

			if (!temp_name.empty()) {
				first_valid_script_step = i;
				return;
			}
		}
	}
}

// Calibrate the plate tray positions
void PlateTray::calibrateByKey(int k, Point3d pos, StrictLock<AbstractCamera> &camLock, AbstractCamera &cam0) {

	switch (k) {

	case keyDelete:
		deleteCalibrationPoint();
		break;

	case keyInsert:
		addCalibrationPoint(pos, camLock, cam0);
		break;

	case keyEnd:
		runCalibration();
		break;

	case keySpace:
		loadCalibrationCoords();
		break;

	case keyBackSpace:
		cout << "Requested manual halt of script execution..." << endl;
		resetPlateSeries();
		break;
	}
}

void PlateTray::addCalibrationPoint(Point3d pos, StrictLock<AbstractCamera> &camLock, AbstractCamera &cam0) {

	// Load from disk
	loadCalibrationCoords();

	// Try to determine position automatically by reading the barcode
	/*
	camLock.lock();
	int row = -1, col = -1;
	bool is_found = cam0.scanForQrCode(camLock);
	if (is_found) {
		row = cam0.getQrRowCol().row;
		col = cam0.getQrRowCol().col;
	}
	camLock.unlock();
	*/

	bool is_found = false;
	int row = -1, col = -1, k = -1;

	// If No valid barcode exists, interrupt live preview and ask user to specify
	MatIndex man_idx = getManualPlateId(is_found);
	row = man_idx.row;
	col = man_idx.col;

	// Validate - is it in bounds?
	if (row < 0 || row > n_rows - 1 || col < 0 || col > n_cols - 1) {
		cout << "FAILED TO ADD CALIBRATION POINT -- (" << row << "," << col << ") IS NOT ON THE TRAY." << endl;
		return;
	}

	// Store the location and physical position
	calibration_ids.push_back(man_idx);
	calibration_coords.push_back(pos);

	// Display all coordinates
	displayCalibrationCoords();

	// Write the updated list to disk
	writeCalibrationCoords();
}

MatIndex PlateTray::getManualPlateId(bool is_found) {

	int row = -1, col = -1, k = -1;

	Mat img_display(Size(640, 480), CV_8UC3, Scalar(0, 0, 0, 0));
	if (!is_found)
		putText(img_display, "Failed to detect Row/Col by barcode...", Point(20, 100), 1, 1.5, colors::yy, 2);
	else
		putText(img_display, "Barcode detected...", Point(20, 100), 1, 1.5, colors::yy, 2);

	putText(img_display, "Enter [2 DIGIT] row number...", Point(20, 150), 1, 1.5, colors::yy, 2);
	imshow("Specify_Plate", img_display);

	if (!is_found) {
		row = waitKeyNum(2, 0, img_display, Point(440, 150),"Specify_Plate");
	}
	else {
		k = waitKey(200);
	}
	
	putText(img_display, to_string(row), Point(440, 150), 1, 1.5, colors::rr, 3);
	putText(img_display, "Enter [2 DIGIT] col number...", Point(20, 200), 1, 1.5, colors::yy, 2);
	imshow("Specify_Plate", img_display);

	if (!is_found) {
		col = waitKeyNum(2, 0, img_display, Point(440, 200),"Specify_Plate");
	}
	else {
		k = waitKey(200);
	}
	
	putText(img_display, to_string(col), Point(440, 200), 1, 1.5, colors::rr, 3);
	putText(img_display, "Any key to continue...", Point(20, 250), 1, 1.5, colors::yy, 2);
	imshow("Specify_Plate", img_display);

	k = waitKey(500);

	// Destroy the window
	destroyWindow("Specify_Plate");

	return MatIndex(row, col);
}

void PlateTray::deleteCalibrationPoint() {

	// Load / refresh the points from disk
	loadCalibrationCoords();

	// Delete only if size is not 0
	if (calibration_coords.size() > 0) {
		calibration_coords.pop_back();
		calibration_ids.pop_back();
	}

	// Display list
	displayCalibrationCoords();

	// Write list
	writeCalibrationCoords();
}



/*
 Edit a specific calirbation plates' coordinate,
 update the file
 */
void PlateTray::editCalibrationCoords(int r, int c, float x, float y, float z) {
	// Load the latest JSON before writing anything so we don't overwrite with stale data
	Config::getInstance().loadFiles();
	json plates = Config::getInstance().getConfigTree("plates");

	// Replace the information in calibration samples
	plates.erase("calibration samples");

	for (int i = 0; i < calibration_ids.size(); i++) {
		json sample;
		sample["row"] = calibration_ids[i].row;
		sample["col"] = calibration_ids[i].col;
		if (calibration_ids[i].row == r && calibration_ids[i].col == c) {
			sample["position"]["x"] = x;
			sample["position"]["y"] = y;
			sample["position"]["z"] = z;
		}
		else {
			sample["position"]["x"] = calibration_coords[i].x;
			sample["position"]["y"] = calibration_coords[i].y;
			sample["position"]["z"] = calibration_coords[i].z;
		}
		plates["calibration samples"].push_back(sample);
	}

	Config::getInstance().replaceTree("plates", plates);

	// Write to disk
	Config::getInstance().writeConfig();
}

/*
 Given all the plates' coordinate for calirbation
 (calibration plate is the plate with min row andn col in each tray),
 generate and store all plates' coordinates in the json file ["plates"]["plates"]
 */
void PlateTray::generateCoordsBasedOnCalibrationCoords() {

	// loadCalibrationCoords();

	// image path ???

	// Load the latest JSON before writing anything so we don't overwrite with stale data
	Config::getInstance().loadFiles();

	// Erase past information of all plates coordinates
	json plates = Config::getInstance().getConfigTree("plates");
	plates.erase("plates");



	// Define the length of interval of row and column between plates
	float colInterval = 62.50;
	float rowInterval = 66.67;
	// cout << "calibration id";
	// cout << calibration_ids.size();
	for (int i = 0; i < 8; i++) {

		int row = calibration_ids[i].row;
		int col = calibration_ids[i].col;

		int width = 0;
		// The tray on one side has size 5*4 and on another side has size 4*4
		if (col == 0) {
			width = 5;
		}
		else if (col == 5) {
			width = 4;
		}
		int length = 4;

		// Add the calibration plate
		json coord;
		coord["row"] = row;
		coord["col"] = col;
		coord["name"] = "";
		coord["img path"] = "";
		coord["type"] = "";
		coord["position"]["x"] = calibration_coords[i].x;
		coord["position"]["y"] = calibration_coords[i].y;
		coord["position"]["z"] = calibration_coords[i].z;
		plates["plates"].push_back(coord);

		// For each 5*4 or 4*4 tray,
		// calculate and add plates other than the calibration plate
		for (int r = row + 1; r < row + length; r++) {
			for (int c = col + 1; c < col + width; c++) {
				json coord;
				coord["row"] = r;
				coord["col"] = c;
				coord["name"] = "";
				coord["img path"] = "";
				coord["type"] = "";
				coord["position"]["x"] = calibration_coords[i].x - c * colInterval;
				coord["position"]["y"] = calibration_coords[i].y - r * rowInterval;
				coord["position"]["z"] = calibration_coords[i].z;
				plates["plates"].push_back(coord);
			}
		}
	}

	Config::getInstance().replaceTree("plates", plates);

	// Write to disk
	Config::getInstance().writeConfig();
}



void PlateTray::setPlateZ(double z) {
	plateMat(plate_curr.row, plate_curr.col)->coord.z = z;
	pos_curr = plateMat(plate_curr.row, plate_curr.col)->coord;
}



void PlateTray::saveMsgfromSocket(string& MsgfromSock) {
	Message_from_socket.clear();
	Message_from_socket = MsgfromSock;
}

void PlateTray::runCalibration() {

	// Save to disk
	loadCalibrationCoords();

	// Trigger MATLAB fitter 
	Mat img_display(Size(640, 480), CV_8UC3, Scalar(0, 0, 0, 0));

	if (calibration_ids.size() < 5) {
		putText(img_display, "Less than 5 points supplied.", Point(20, 100), 1, 1.5, colors::yy, 2);
		putText(img_display, "Calibration will not proceed. ", Point(20, 150), 1, 1.5, colors::yy, 2);
		putText(img_display, " Any key to continue... ", Point(20, 200), 1, 1.5, colors::yy, 2);

		imshow("Preview", img_display);
		waitKey(0);
		return;
	}
	else {
		putText(img_display, "Please wait for calibration fits to complete.", Point(20, 100), 1, 1.5, colors::yy, 2);
		putText(img_display, "This could take a moment...", Point(20, 150), 1, 1.5, colors::yy, 2);
		putText(img_display, "After the operation completes", Point(20, 250), 1, 1.5, colors::yy, 2);
		putText(img_display, "close ALL pop-ups to continue.", Point(20, 300), 1, 1.5, colors::yy, 2);

		imshow("Preview", img_display);
		waitKey(200);
	}

	string result = executeExternal(filenames::calibration_program.c_str());

	if (!result.compare("NOT FOUND")) {
		img_display = Mat(Size(640, 480), CV_8UC3, Scalar(0, 0, 0, 0));
		putText(img_display, "Failed to run calibration program:", Point(20, 100), 1, 1.5, colors::yy, 2);
		putText(img_display, filenames::calibration_program, Point(20, 150), 1, 1.5, colors::yy, 2);
		putText(img_display, " Any key to continue... ", Point(20, 200), 1, 1.5, colors::yy, 2);
		imshow("Preview", img_display);
		waitKey(0);
		return;
	}


}

void PlateTray::displayCalibrationCoords() {

	cout << "\n\nCALIBRATION POINTS:";
	cout << "\n---------------------------------------\n";

	for (int i = 0; i < calibration_ids.size(); i++) {
		cout << calibration_ids[i].row << "\t";
		cout << calibration_ids[i].col << "\t";
		cout << calibration_coords[i].x << "\t";
		cout << calibration_coords[i].y << "\t";
		cout << calibration_coords[i].z << "\n";
	}
	cout << "\n---------------------------------------\n\n";

}

void PlateTray::writeCalibrationCoords() {

	// Load the latest JSON before writing anything so we don't overwrite with stale data
	Config::getInstance().loadFiles();
	json plates = Config::getInstance().getConfigTree("plates");

	// Replace the information in calibration samples
	plates.erase("calibration samples");

	for (int i = 0; i < calibration_ids.size(); i++) {
		json sample;
		sample["row"] = calibration_ids[i].row;
		sample["col"] = calibration_ids[i].col;
		sample["position"]["x"] = calibration_coords[i].x;
		sample["position"]["y"] = calibration_coords[i].y;
		sample["position"]["z"] = calibration_coords[i].z;
		plates["calibration samples"].push_back(sample);
	}

	Config::getInstance().replaceTree("plates", plates);

	// Write to disk
	Config::getInstance().writeConfig();
}

void PlateTray::loadCalibrationCoords() {

	calibration_ids.clear();
	calibration_coords.clear();

	// read each sample in the list of samples
	json plates = Config::getInstance().getConfigTree("plates");
	for (auto sample : plates["calibration samples"]) {
		int r, c, x, y, z;
		r = sample["row"].get<int>();
		c = sample["col"].get<int>();
		x = sample["position"]["x"].get<int>();
		y = sample["position"]["y"].get<int>();
		z = sample["position"]["z"].get<int>();

		calibration_ids.push_back(MatIndex(r, c));
		calibration_coords.push_back(Point3d(x, y, z));
	}

	// Display
	displayCalibrationCoords();
}

Point3d PlateTray::getPlateCoords(MatIndex idx) const {
	return plateMat(idx.row, idx.col)->coord;
}

Point3d PlateTray::getPlateCoords() const {

	MatIndex temp_coords(0,0);

	return getPlateCoords(temp_coords);
}

Point3d PlateTray::getSpecificPlateCoords() {

	// Ask the user for a specific plate
	MatIndex mat_idx = getManualPlateId(false);

	// Validate - is it in bounds?
	if (mat_idx.row < 0 || mat_idx.row > n_rows - 1 || mat_idx.col < 0 || mat_idx.col > n_cols - 1) {
		cout << "FAILED TO MOVE TO PLATE -- (" << mat_idx.row << "," << mat_idx.col << ") IS NOT ON THE TRAY." << endl;
		return Point3d(-1,-1,-1);
	}

	// If so, retrieve the plate ID
	else {
		return getPlateCoords(mat_idx);
	}
}


void PlateTray::changeScriptId(MatIndex id) {
	script[script_step].id = id;
	return;
}

void PlateTray::changeScriptArgs(int number, string value) {
	script[script_step].args[number] = value;
}

void PlateTray::changeScriptArgs(std::vector<string> Args) {
	script[script_step].args = Args;
}

void PlateTray::resetScriptArgs() {
	script[script_step].args = {};
}




// Getter
Point3d PlateTray::getCurrentPosition() const { return pos_curr; }
MatIndex PlateTray::getCurrentId() const { return plate_curr; }
string PlateTray::getCurrentPlateName() { return name_curr; }
int PlateTray::getCurrentScriptStep() { return script_step; }
PlateTrayScriptStep PlateTray::getCurrentScriptStepInfo() const { return script[script_step]; }
bool PlateTray::atFirstValidStep() { return first_valid_script_step == script_step; }
int PlateTray::getTotalScriptSteps() { return (int) script.size(); }
bool PlateTray::getPlateStateWasRunning() { return plate_script_was_running; }
bool PlateTray::getPlateStateIsRunning() { return plate_script_is_running; }
bool PlateTray::isMoveStep() const { return script[script_step].step_action.find("Move") != 0; }
vector<string> PlateTray::getAllPlatePaths() const { return all_plate_parent_paths; }


int PlateTray::getNextExptHour() const {

	// Return negative if there are no hours
	if (expt_hours.size() == 0)
		return -9;

	// Return negative if there are no positive hours (e.g. continuous run, requires hours sorted at loading)
	if (expt_hours[expt_hours.size()-1]<0)
		return -9;

	// Get current hour
	double hr = getCurrentHour();

	// Find first hour greater than current hour (requires hours to be sorted at loading)
	int next_hr = -9;
	for (int i = 0; i < expt_hours.size(); i++) {
		if (expt_hours[i] > hr) {
			next_hr = expt_hours[i];
			break;
		}
	}

	// If none was found, loop to the next day
	for (int i = 0; next_hr < 0 && i < expt_hours.size(); i++) {
		if (expt_hours[i] >= 0) {
			next_hr = expt_hours[i];
			break;
		}
	}
	return next_hr;
}

double PlateTray::getTimeToNextExpt() const {

	// Get current hour and next hour
	double hr = getCurrentHour();
	double min = getCurrentMinute();
	double sec = getCurrentSecond();
	double next_hr = (double) getNextExptHour();

	// Compute 
	double duration = 500;

	if (next_hr > hr) {
		duration = next_hr - (hr + min / 60 + sec / 3600);
	}
	else if (next_hr > 0) {
		duration = next_hr + 24 - (hr + min / 60 + sec / 3600);
	}
	/// Else duration is 500 hrs because no experiments are coming

	return duration;
}


/*

	NON-MEMBER HELPER FUNCTIONS

*/




//// Thread function to cycle script steps and/or plates for the WWExperiment
//
//void cycleWormScript(PlateTray &Tr, OxCnc &Ox, OxCnc& Grbl2,
//	StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock,
//	int &kg, WormPick& Pk, WormFinder& worms, CameraFactory& camFact)
//{
//
//	for (;;) {
//
//		// Check for exit request
//		boost_sleep_ms(25);
//		boost_interrupt_point();
//
//
//		/* -------------- USER MANUAL REQUESTS via BUTTON PRESS ------------------- */
//
//		// Check if manual Ox CNC  movement was requested
//		if (!OxCnc::move_secondary_manual && Ox.isConnected() && Ox.isRecognizedKey(kg)) {
//			///oxLock.lock(); /// Currently unnecessary to lock since all write-access in cyclePlates... thread
//			Ox.goToByKey(kg, oxLock, true);
//			///oxLock.unlock();
//		}
//		else if (OxCnc::move_secondary_manual && Grbl2.isConnected() && Grbl2.isRecognizedKey(kg)) {
//			///grbl2Lock.lock();
//			Grbl2.goToByKey(kg, grbl2Lock, true);
//			///grbl2Lock.unlock();
//		}
//
//		// Check if manual DYNAMIXEL movement was requested
//		Pk.movePickByKey(kg, Grbl2);
//
//		// Check whether the user requested that the pick be centered now
//		if (kg == keyAsterisk) {
//			cout << "Centering pick...\n";
//			Pk.centerPick(Ox, Grbl2, worms);
//		}
//
//		// Flush the keypress to ready for a new one.
//		kg = -5; 
//
//
//		/* --------------------- SCRIPT-DEFINED ACTIONS  ----------------------- */
//
//
//		// Take no action if it's not time to run (or currently running). Note that red will be turned on prior to experiment
//		if (!Tr.checkForStartTime())
//			continue;
//
//		// Get the plate name and output path
//		Tr.updateCurrentPath(); /// Updates save_path
//								//plate_name = Tr.getCurrentPlateName();
//
//		// If it is time to run and we're just starting (e.g. on the first step), home the CNC first
//		/*if (Tr.atFirstValidStep() && Ox.isConnected())
//			Ox.homeCNC(false);*/
//
//		// If the plate tray is in DUMMY mode, skip the actual experiment, otherwise run it
//		//	Experiment is also forced to run if GRBL is not connected. Otherwise it runs through the script and does nothing.
//		bool success = false;
//
//		if (!Tr.dummy_mode || !Ox.isConnected()) {
//
//			// Run script step
//			success = executeScriptAction(Tr,Tr.getErr(),Pk,Ox,Grbl2,oxLock,grbl2Lock,worms,kg, camFact) &&
//							Tr.getPlateStateIsRunning();
//			boost_sleep_ms(100);
//
//			// Abort script if any step files
//			if (!success) {
//				cout << "Script stopped after step: " << 
//					Tr.getCurrentScriptStepInfo().step_action << endl;
//				Tr.resetPlateSeries();
//			}
//		}
//
//		/* 
//			Move to the next step and/or plate in the script. 
//			- If this is the last step / plate step will be set back to 0.
//			- If action did not complete successfully, the script has been reset and we shouldn't increment here.
//			- If there was no plate at the requested position,
//		*/
//		if (success)
//			Tr.setNextPlate();
//
//	}
//}


int PlateTray::getNumColumns() const {
	return n_cols;
}

int PlateTray::getNumRows() const {
	return n_rows;
}

ErrorNotifier& PlateTray::getErr() const {
	return  Err;
}

bool PlateTray::getListen_to_Socket() const {
	return listen_to_Socket;
}

std::string PlateTray::getRaw_ip_address() const {
	return raw_ip_address;
}

unsigned short PlateTray::getPort_num() const {
	return port_num;
}


