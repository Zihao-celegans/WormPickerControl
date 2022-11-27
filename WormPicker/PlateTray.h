/*
	PlateTray.h
	Anthony Fouad
	Fang-Yen Lab
	Fall 2018

	Class that governs the names, locations, and other parameters of plates on the CNC robot.
*/

#pragma once
#ifndef PLATETRAY_H
#define PLATETRAY_H

// Anthony's includes
#include "MatIndex.h"
#include "PlateTrayScriptStep.h"
#include "Script.h"
#include "OxCnc.h"
//#include "WormPick.h"
#include "Camera.h"
#include "ErrorNotifier.h"
#include "AbstractCamera.h"
#include "CameraFactory.h"
#include "WorMotel.h"


// OpenCV includes
#include "opencv2\opencv.hpp"

// Boost Matrix
#include "boost/numeric/ublas/matrix.hpp"
#include "boost/optional.hpp"

// Standard includes
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <memory>

struct plate {
	//PlateMap tileMap;
	std::string name;
	std::string fullPath;
	cv::Point3d coord;
	std::string type;
};

class PlateTray {

public:

	// Constructor: Sets file names containing parameters
	PlateTray(std::ofstream &dl, ErrorNotifier &ee, std::vector<std::shared_ptr<std::string>>& sps,
			  std::vector<int> cns, bool &dm, std::string& _cpt, bool _isww);
	//Copy constructor
	PlateTray(const PlateTray& Tr);
	// Load text parameters from disk (everything, including script, but excluding current state)
	bool loadParams();


	// Load or save state from disk
	void loadPlateState();
	void savePlateState();
	bool setupPlateScript();
	void loadPlateScript();
	void autoGeneratePlateScript();

	// Start (or resume) a series of plate movements
	bool startPlateSeries();

	// Determine whether it is time to start a series of plate movements or if it's already running
	bool checkForStartTime();
	bool checkIfPlateSeriesIsRunning();
	void setPlateSeriesRunning();  /// Starts plate series from current state, does not re-load state or plate info from disk
	void stopPlateSeriesRunning(); /// Stops (pauses) the plate series
	void resetPlateSeries();		/// Stop and set script step to 0.

	// Set the next plate as current plate or declare that we have finished (true) OR declare that we have finished the series (false)
	bool setNextPlate();
	bool checkCurrentPlate();	/// Check that the current plate is valid (decrement step and then setNextPlate() )
	void updateStepInfo();		/// Extract information about the current step from the script
	void updateCurrentPath();	/// Update the current path(s), which is a vector of pointers that are shared with main
	bool validateStepInfo(MatIndex test_plate); /// Determine whether the requested plate index exist on the PlateTray
	void findFirstValidScriptStep(); /// Find the first step at which a plate is present and can be used

	// Calibration of the plate tray (rectangular for now)
	void calibrateByKey(int k, cv::Point3d pos, StrictLock<AbstractCamera> &camLock, AbstractCamera &cam0);
	void addCalibrationPoint(cv::Point3d pos, StrictLock<AbstractCamera> &camLock, AbstractCamera &cam0);
	MatIndex PlateTray::getManualPlateId(bool is_found);
	void deleteCalibrationPoint();
	void editCalibrationCoords(int r, int c, float x, float y, float z);
	void generateCoordsBasedOnCalibrationCoords();
	void runCalibration();
	void displayCalibrationCoords();
	void writeCalibrationCoords();
	void loadCalibrationCoords();
	cv::Point3d getPlateCoords(MatIndex idx) const; /// Get coordinates of specified row and column
	cv::Point3d getPlateCoords() const; /// Get coordinates of row and column supplied by user
	cv::Point3d getSpecificPlateCoords();
	void changeScriptId(MatIndex id);
	void changeScriptArgs(int number, std::string value); //changing script arguments
	void changeScriptArgs(std::vector<std::string> Args); // overloaded function for changing script arguments
	void resetScriptArgs(); // Reset script arguments to be all 0

	// Getters
	cv::Point3d getCurrentPosition() const;
	MatIndex getCurrentId() const;
	std::string getCurrentPlateName();
	int getCurrentScriptStep();
	PlateTrayScriptStep getCurrentScriptStepInfo() const;
	bool atFirstValidStep();					/// Check if we are at the first VALID script step (e.g. step in which a plate is there, might not be step 0...)
	int getTotalScriptSteps();
	bool getPlateStateWasRunning();
	bool getPlateStateIsRunning();
	bool isMoveStep() const;
	std::vector<std::string> getAllPlatePaths() const; /// Find all the unique paths (e.g. parent directories) that plate data is being recorded to
	int getNextExptHour() const;
	double getTimeToNextExpt() const;
	int getNumColumns() const;
	int getNumRows() const;
	ErrorNotifier& getErr() const;
	bool getListen_to_Socket() const;
	std::string getRaw_ip_address() const;
	unsigned short getPort_num() const;
	//boost::numeric::ublas::matrix<boost::optional<plate>> getPlateMat() { return plateMat; }

	// Setter
	void setPlateZ(double z);


	// Handling communication through socket
	void saveMsgfromSocket(std::string& MsgfromSock);




	// Public varible: Whether to use the tray in dummy mode
	bool &dummy_mode;
	cv::Point3d currentOffset;	/// Storage variable for current offset updated by script. NO EFFECT INTERNALLY ON PLATE POSITIONS.


	// Object of WorMotel
	WorMotel WrMtl;


protected:

	// Data read in from files user params
	int n_rows;										/// Number of rows (Y locations)
	int n_cols;
	// std::vector<int> n_cols;						/// Number of columns  (X locations) in each row. N_cols.size() must equal N_rows
	std::vector<int> expt_hours;					/// Times at which to start an experiment, generally [0,12]
	
	// Data read in from plate name and positions file (size: n_rows x ncols[row_curr]). Info about all plates on tray
	//std::vector<std::vector<std::string>> plate_names;		/// Name of each plate in the series; fname_plate_names
	//std::vector<std::vector<std::string>> plate_full_paths;	/// Full path of each plate including user path and plate name subfolder; fname_plate_names
	std::vector<std::string> all_plate_parent_paths;		/// All parent paths (one level above plate paths)
	std::vector < std::string > all_plate_ref_files;		/// Full path of ROI reference file (.mat) for each parent path. Used for analysis.
	//std::vector<std::vector<cv::Point3d>> plate_coords;		/// Coordinates for each plate; fname_plate_positions

	boost::numeric::ublas::matrix<boost::optional<plate>> plateMat;

	// Data used for calibration (user specifies coordinates and row/col)
	std::vector<cv::Point3d> calibration_coords;
	std::vector<MatIndex> calibration_ids;

	// Data read in from the actions script. Info about the order in which to do steps. valid step names
	Script script;										/// Vector of script steps makes the script
	int script_step;									/// Script step we are currently on
	int first_valid_script_step;						/// First step that can be performed because there is a plate at the location

	// Data updated dynamically
	bool plate_script_was_running;						/// Whether the script was running in the state file loaded from disk
	bool plate_script_is_running;						/// Whether the script is currently running
	bool plate_action_is_running;						/// Whether the action to be performed on the current plate is running
	MatIndex plate_curr;								/// Current row and column plate
	std::string name_curr;								/// Current plate name
	std::vector<std::shared_ptr<std::string>>& full_path_curr;			/// Current full path
	std::vector<int> cam_nums;							/// List of the camera number at each index
	cv::Point3d pos_curr;								/// Current plate position
	std::ofstream &debug_log;							/// Reference to the debugging text log
	ErrorNotifier &Err;									/// Reference to the error notifier. Somewhat redundant with debug_log but the original functionality is needed
	std::string& current_plate_type;					/// Reference to the type of the current plate from the JSON
	bool is_wormwatcher;								/// True if running WW, False if running picker

	// Socket objects for script execution
	bool listen_to_Socket;								/// Flag indicate whether receive script commands from Python through socket (if true), or simply execute scripts inside C++
	std::string raw_ip_address;							/// Raw IP address for creating socket
	unsigned short port_num;							/// Port number for creating socket for receiving commands

	// Communications through socket
	std::string Message_from_socket =					/// String holding the command message receiving from the socket
		"action	0	0	DummyScript	0	0	0	0	0	0	0	0	0	0"; // Initialized to some value 
																			// since the constructor does not like empty script before receiving anything

	
};

//// Non-member helper functions
// This function has been moved to ScriptActions.h in order to eliminate circular dependencies when adding LidHandler.h to the WormPick.h 
// (i.e. PlateTray.h was importing WormPick.h, which was importing LidHandler.h, which was importing PlateTray.h... so I removed Wormpick.h from PlateTray.h)
//void cycleWormScript(PlateTray &Tr, OxCnc &Ox, OxCnc& Grbl2,
//	StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock,
//	int &kg, WormPick& Pk, WormFinder& worms, CameraFactory& camFact);

#endif
