/*
	OxCnc.h
	Anthony Fouad

	Class declaration for OX CNC robot control.
	This class uses EXTERNAL locking (see Camera.cpp's usage for example)
*/
#pragma once
#ifndef OXCNC_H
#define OXCNC_H

// Standard includes
#include <string>
#include <iostream>
#include <fstream>

// Serial port includes
#include "BufferedAsyncSerial.h"

// OpenCV includes
#include "opencv2\opencv.hpp"

// Boost includes
#include "boost\assert.hpp"
#include "boost\thread.hpp"
#include "boost\thread\lockable_adapter.hpp"

// Anthony's includes
#include "AnthonysTimer.h"
#include "StrictLock.h"
#include "LabJackWB.h"
#include "ErrorNotifier.h"
#include "HardwareSelector.h"
#include "WormFinder.h"


class OxCnc : public boost::basic_lockable_adapter<boost::mutex>
	
{

public:

	// Constructor with default arguments
	OxCnc(std::string device_name, ErrorNotifier &ee, HardwareSelector &_use, bool iug, bool block_initial_homing = true, OxCnc* grbl2_ptr = nullptr);

	// Destructor the cleans up heap-allocated Serial port object
	~OxCnc();
	
	// Setup and validation
	void homeCNC(bool force = false, bool blockflag = true);
	void validateConnection();
	bool connectToSerial();
	bool reconnectSerial();

	// High level movement functions
	bool goToAbsolute(double x, double y, double z, bool blockflag = false, int waitTime = 25);
	bool goToAbsolute(std::vector<double> newpos, bool blockflag = false, int waitTime = 25);
	bool goToAbsolute(cv::Point3d newpos, bool blockflag = false, int waitTime = 25);
	bool goToAbsoluteWithRaise(cv::Point3d newpos, StrictLock<OxCnc> &grblLock, bool blockflag = false, int waitTime = 25);
	bool goToRelative(double dx = 0, double dy = 0, double dz = 0, double multiplier = 1.0, bool blockflag = false, int waitTime = 25);
	bool goToRelative(std::vector<double> dv, double multiplier = 1.0, bool blockflag = false, int waitTime = 25);
	bool goToRelative(cv::Point3d dv, double multiplier = 1.0, bool blockflag = false, int waitTime = 25);
	bool goToByKey(const int &keynum, StrictLock<OxCnc> &grblLock, bool block_flag = true);
	bool jogOx(double dx, double dy, double dz);
	void abortJog();
	bool moveTowardTargetedPoint(cv::Point2f tracked_centroid, cv::Point pickup_location, double track_radius, double jog_interval = 0.021, int max_allow_offset = 2);
	bool raiseGantry();
	bool lowerGantry();
	//void liftOrLowerLid(bool lifting = true);
	//void lowerLid(bool return_to_plate = true);
	void synchronizeXAxes(double OxX);
	void updateOffset(double OxX);

	// Getter functions and serial reading
	double getX() const;
	double getY() const;
	double getZ() const;
	double getXgoal() const;
	double getYgoal() const;
	double getZgoal() const;
	cv::Point3d getXYZ() const;
	cv::Point3d getXYZgoal() const;
	double getInt() const;
	bool isInState(std::string state_str, std::string& return_str, bool update_alarm = true); /// True if the grbl return text contains state_str. Updates return_str
	bool isInState(std::string state_str, bool update_alarm = true); /// Check only, do not return message. 

	bool isMoving();			/// True if GRBL board is currently in "Run"
	bool isDoneHoming();		/// True if the Grbl board is currently is "Idle". Return command doesn't work during homing sequence, and may be unreliable. 
									/// More accurate is to set it to run after and wait for the run to start and stop.
	bool isHoming() const;			/// Does NOT queury serial, only requests object's current state.
	bool isAnything();			/// True if the GRBL sends back any normal operation response
	bool isConnected() const;	/// True if serial port connected successfully
	cv::Point3d queryGrblPos(); /// Returns the position from the GRBL board as a 3d point
	bool verifyGrblPos(cv::Point3d test_pt, double precision = 0.5);		
								/// Returns true if all elements of test_pt are within +/- precision from grbl_pos
	void writeGrblLimits();		/// Overwrite grbl settings file associated with the current object on the disk. 
	//cv::Point3d getPosLidShift() const;/// Return the shift position of the lid. Note that the Z shift is zero, get separately the absolute coord
	//double getZPlateHeight() const; /// Get absolute Z coordinate for plate height

	long getLabJackHandle() const;
	bool isRecognizedKey(int k);		/// True if the key press (usually k or kg) is associated with a manual movement of the stage, false otherwise. Does not actually move.
	//bool getLidIsRaised() const;	/// Determine whether the lid is currently raised

	// Setter functions
	void setRelSpeed(double speed);		/// Speed [0 -> 1] of X and Y stages during absolute movement (not jogging)

	// Public variables 
	LabJackWB lj;						/// LabJack pins, mainly for wormpicker user
	static bool move_secondary_manual;			/// Whether or not manual movements should be rou ted to the secondary gantry

protected:


	// Low level movement. Do not call except in goToAbsolute(). Write position and wait for completion if requested
	void writeSerialPos(double x, double y, double z, bool blockflag, int waitTime);

	// Translate GRBL error and alarm codes, which are not automatically verbose in 1.1
	std::string translateGrblCode(std::string test_str);

	// Convert and translate ALL current GRBL errors to a single string suitable for printing
	std::string formatGrblErrors();

	// Figure out which 3 elements in pos_stored to edit, depending on which key F1-F4 was pressed
	int keyPressIdx(int keynum);
	cv::Point3d keyPressPos(int keynum);

	// Protected variables
	ErrorNotifier &Err;
	double man_int;				/// Interval to use for manual movement
	std::string port_name;			/// "e.g. COM7"
	std::string last_return;			/// Last message returned form GRBL to PC. 
	std::string device_name;
								/// File name of the parameters file for Ox CNC
								/// File name of the file that contains a stored position
	cv::Point3d pos, pos_max, pos_min, pos_exit, pos_plate;//, pos_lid, pos_lid_shift;
	boost::optional<cv::Point3d> pos_mem;
	cv::Point3d stored_positions[4];
	//std::vector<double> pos, pos_limits, pos_stored, pos_mem, pos_plate, pos_lid, pos_lid_shift, pos_exit;
								/// Current X, Y, and Z coordinates. [X, Y, Z]
								/// Limits specified as [xMin,xMax,yMin,yMax,zMin,zMax]
								/// X, Y, and Z position "stored" by user, representing a key position.
								/// X, Y, and Z position "remembered" just before raising carraige so it can be lowered later
								/// X, Y, and Z position of the current plate
								/// X, Y, and Z position of the current plate's lid for the grabber (related by the _shift)
								/// X and Y position of the lid lifting suction RELATIVE to the camera. 
								///		Positive values go TOWARDS the lid lifting position.
								///		Negative values go BACK to the camera-centered position
								///		Use alongside z_lid_height, the absolute height of all plate lids
	cv::Point3d pos_grbl;		/// Position queried from GRBL board. Set by queryGrblPos() or verifyGrblPos()
	cv::Point3d pos_goal;		/// Goal position, set when goToAbsolute() is called before movement actually starts.
	std::vector<double> max_speed;	/// X, Y, and Z maximum speeds in GRBL
	//double z_lid_height;		/// Height of plate lids
	double current_speed;		/// Current speed, as a fraction of max_speed;
	double jog_speed;			/// Speed when jogging.
	double x_sync_offset;		/// Offset between X axes of Grbl2 (undercarriage light) and Main OX. SHould be 0 for Ox and ~21 for Grbl2
	bool isconnected;			/// Serial connected, default no.
	bool is_homing;				/// true when in the homing function
	bool move_xy_first;			/// True if we need to FIRST move to the XY position (at maximum height), e.g. when recovering from fault
	//bool lid_is_raised;			/// True if (any) plate lid is currently being held in suction
	bool is_upper_gantry;		/// True if specified by input argument. Upper gantry is capable of lid lifting, gantry raising, etc. 
	BufferedAsyncSerial *Serial;/// Serial port object. Must be initialized as blank in constructor.
	bool waitingforkeypress;	/// User presses 0 to indicate that an absolute position is ready to be saved, 
									 /// setting this flag to true. Then the user needs to press F1 through F4. 
	std::vector<std::string> grbl_error;		/// Clear if no error, otherwise contains error text from serial port. Can contain multiple errors
	long lab_jack_handle;
	bool turn_off_red_after;
	int baud_rate;
	AnthonysTimer T_last_home; /// Timer used to calculate how long ago the robot homed.
	double skip_homing_if_s_ago; /// Skip homing if last home was >= this many s ago
	HardwareSelector &use;		/// Whether to use the CNC
	OxCnc* grbl2_ptr;			/// Pointer to the second Grbl so that movement commands can be routed to the correct grbl motor
};

/*
	NON-MEMBER FUNCTIONS RELATED TO THE GRBL MOVEMENTS
*/
bool centerTrackedWormToPoint(WormFinder& worms, OxCnc& Ox, cv::Point center_loc, int track_mode=2, bool wait_if_lost = true, bool use_mask_rcnn = false);
bool helperCenterWorm(AnthonysTimer &Ta, double time_limit_s, WormFinder& worms, OxCnc &Ox);
bool helperCenterWormCNN(AnthonysTimer &Ta, double time_limit_s, WormFinder& worms, OxCnc &Ox);
bool helperCenterWormCNNtrack(AnthonysTimer &Ta, double time_limit_s, WormFinder& worms, OxCnc &Ox);
bool helperKeepWormCentered(AnthonysTimer &Ta, double time_limit_s, WormFinder& worms, OxCnc &Ox);

#endif