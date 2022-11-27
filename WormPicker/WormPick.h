/*
	WormPick.h
	Anthony Fouad
	Fang-Yen Lab
	April 2018

	Object containing information about the worm pick
*/
#pragma once
#ifndef WORMPICK_H
#define WORMPICK_H

// Standard includes
#include <vector>
#include <deque>
#include <cmath>

// Boost includes
#include "boost\thread.hpp"
#include "boost\asio.hpp"

// OpenCV includes
#include "opencv2\opencv.hpp"
#include "opencv2\dnn.hpp"

// Anthony's includes
#include "AnthonysTimer.h"
#include "AnthonysCalculations.h"
#include "DynamixelController.h"
#include "AnthonysKeyCodes.h"
#include "OxCnc.h"
#include "WormFinder.h"
#include "ThreadFuncs.h"
#include "HardwareSelector.h"
#include "ErrorNotifier.h"
#include "Worm.h"
#include "DnnHelper.h"
#include "StrictLock.h"
#include "ActuonixController.h"
#include "LidHandler.h"

class WormPick {

public:

	// Constructor
	WormPick(bool &ipa, HardwareSelector _use, ErrorNotifier &_Err);						/// Blank pick object with NO intialized dynamixel
	//WormPick(std::string fDynParams, std::string fPickupLoc, bool& ipa, HardwareSelector _use, ErrorNotifier &_Err);	/// Usable pick object WITH initialized dynamixel

	// Destructor
	~WormPick();

	// Load and save pick info
	cv::Point loadPickupInfo();
	void savePickupInfo(cv::Point);
	void savePickupInfo();
	void clearPick();


	// Segment the pick
	bool segmentPick(const cv::Mat &imggray, const WormFinder &worms, cv::Mat& imgfilt);
	bool segmentLoop(const cv::Mat &imggray, const WormFinder &worms, cv::Mat& imgfilt);
	bool DNNsegmentLoop(const cv::Mat &imggray, const WormFinder &worms, cv::Mat& imgfilt, cv::dnn::Net net);
	void computePickTipInfo(const cv::Mat &imgfilt, const WormFinder &worms, int h, int w);
	void segmentPickCnn(const cv::Mat& imggray, const cv::dnn::Net& net, const WormFinder& worms, bool run_in_thread = true);
	
	double threshold_with_mask(const cv::Mat &src, cv::Mat &dst, double thresh, double maxval, int type, const cv::Mat &mask = cv::Mat());
	double otsu_8u_with_mask(const cv::Mat src, const cv::Mat &mask);

	// Pick up with sweep (DEPRECATED)
	bool pickSweep(OxCnc& Grbl2, int sweep_dir);

	// Tweak the centering of the pick by 1 step in x and y
	bool fineTuneCentering(bool do_x = true, bool do_y = true);
	bool fineTuneCentering(double x_target_rel, double y_target_rel, bool do_x = true, bool do_y = true);


	// raise or lower the pick, checking focus and touch
	void lowerPickToFocus(OxCnc &Grbl2, long lngHandle); /// Lower until either focus or touch
	void lowerPickToTouch(OxCnc &Grbl2, long lngHandle, bool power_cycle = true, std::string plate_id = "-1-1"); /// Lower purely until touch
	void raisePickToFocus(OxCnc &Grbl2);

	void localizeAndFocus(OxCnc &Grbl2, long lngHandle);

	void WormPick::pickWander(OxCnc& Ox, OxCnc &Grbl2, WormFinder& worms);

	void readyPickToStartPosition(OxCnc &Grbl2);
	void readyPickToStartPositionThread(OxCnc &Grbl2, WormPick &Pk);

	int pickOrDropOneWorm(OxCnc& Ox, OxCnc &Grbl2, WormFinder& worms,
														int dig = 0,
														int sweep = 0,
														double pause = 0,
														bool restore_height_after = false,
														bool integrate_worm_centering = false,
														pick_mode selectionMode = AUTO,
														bool verify_drop_over_whole_plate = false,
														std::string pick_id = "-1-1",
														int num_expected_worms = 1);
	
	bool pickOrDropOneWormREFERENCE(OxCnc& Ox, OxCnc &Grbl2, WormFinder& worms,
														int dig = 0,
														int sweep = 0,
														double pause = 0,
														bool restore_height_after = false,
														bool integrate_worm_centering = false,
														pick_mode selectionMode = AUTO);

	// Calibrate tip position in FOV (mapping tip position in FOV with servo motor positions)
	bool calibratePick(OxCnc& Grbl2, int range, int step_sz);

	// Get the boundary of calibrated region
	void updateCaliMapBound();

	// Check whether the target point is located outside of calibration map
	bool withinCalibrMap(const cv::Point targ_pt);

	// Load the pick calibration data from file
	bool CaliMapfromfile(const std::string pFile);

	// Move the pick to target position according to the calibration map
	bool MovePickbyCalibrMap(OxCnc& Grbl2, const cv::Point targ_pt, const std::string& history);
	bool compareDist(std::pair<cv::Point, cv::Point3i> a, std::pair<cv::Point, cv::Point3i> b);

	// Writepick history data to file
	void writePickHis(const std::string hFile, const cv::Point target_pos, const cv::Point real_pos, const cv::Point corrected_pos, const std::string success);
	// Read data from history file and compute desired information
	void getPickAdjHist(int& cx, int& cy, int& cz, const std::string& hFile, int len_his); // Compute adjustment term from history
	void getPickAdjCurFrm(int& cx, int& cy, int& cz, double multi_x = 1, double multi_y = 1, double multi_z = 1); // Compute adjustment term by current frame


	void heatPick(OxCnc &Grbl2, int DacNum, int secs_to_heat, double secs_to_cool);
	void heatPickInThread(OxCnc &Grbl2, WormPick &Pk, int DacNum, int secs_to_heat, double secs_to_cool);
	void powerCycleCapSensor(long lngHandle);

	// Find the pick 
	void centerPick(OxCnc& Ox, OxCnc& Grbl2, WormFinder& worms);
	void correlateServoToImage(); /// Find out how much movement on servos 1, 2, and 3 moves the pick tip

	// THREAD FUNCTION: Test lowering-camera offset
	void testCameraOffset(OxCnc *Grbl2, WormFinder *worms);

	// THREAD FUNCTION: Read AIN0
	void readLabJackAIN0(long lngHandle);



	// Getter 
	bool pickTipVisible() const;					/// Whether pick tip is visible
	bool pickTipDown() const;						/// Whether pick tip is down
	std::vector <cv::Point> getPickCoords() const;		/// List of coordinates of the pick
	cv::Point getPickTip() const;					/// Coordinates of the pick tip
	int getServoPos(int id);						/// Low level, Return Position of servo "id" or return -1 if none match id.
	int getPickStart1();							
	int getPickCenter2();
	int getPickCenter3();
	int getPickupAngle();
	double getPickupHeight(); // focus_height, where the pick tip is in focus OR touching agar
	int getGrblCenterY();
	int getGrblCenterZ();
	bool getFinishedCenter();
	int getIntensity();
	double getPickGradient(bool filtered_flag = true) const;
	double getLocalMaxChange() const;
	cv::Rect getCaliRegion() const;
	cv::Point getTargetPt() const;
	ActuonixController getActx() const { return Actx; };
	//LidHandler& getGrabbers() { return lid_grabbers; };

	// Setter
	void resetIntensity();
	void movePickByKey(int &keynum, OxCnc& Grbl2);
	bool moveServo(int id, int pos, bool relflag, bool blockflag = false, bool suppressOOBwarn = false);  /// Low level, move a specific servo to a 
																			   /// specific position (relative or absolute) and 
																			   /// optionally suppress the out-of-bounds warning
	bool resetServo(int id);								/// Send a servo to its maximum
	bool resetServoToMin(int id);
	bool movePickToStartPos(OxCnc& Grbl2, bool move_actx = true, bool directly_to_focus_height = false, int servo1_offset = 0, bool careful = true); /// Move the pick to the position needed to start lowering the pick
	bool movePickAway();									/// Move pick out of the way
	bool movePickSlightlyAway();							/// move pick just a little bit out of the way
	bool movePickSlightlyAbovePlateHeight(bool move_pick_out_of_view = false);				/// Moves the pick to a level just above the height of the plates. -- ALERT: This must be calibrated for the hardware
	bool VmovePickSlightlyAway(int stepsz1, int stepsz2, int stepsz3, bool move_above_edge);					/// Raise pick just a little bit up from the agar (for VquickPick)
	void setPickCenter3();									/// Set the current position of motor 2 to be the center
	void setPickCenter3(int setpos);						/// Set the specified angle to be motor 2 start position
	void setPickCenter(int s1, int s2, int s3, double gy, double gz, double fh);				/// Set the "Center" motor positions for the pick
	void setFinishedCenter(bool finished) { finished_centering = finished; }
																/// servo 1 - the start position for lowering
																/// servo 2,3 - Positions of servos 2 & 3 that center the pick

	void setPickStartServo1(int setpos);					/// Set the specified angle to be the motor 1 start position 
															///	for lowering 
	void setPickupAngle(int setpos);
	void stopLowering();

	void setcalibr_servo_coords(int start_pos1, int start_pos2, int start_pos3, int range, int step_sz);
	void setcalibr_tip_coords(const std::vector<cv::Point>& vec_pts);
	void settip_servo_map(const std::vector<cv::Point>& tip_coords, const std::vector<cv::Point>& tip_coords_conv, const std::vector<cv::Point3i>& servo_coords, bool write_flag = false);


	// Public variables: 
	bool &in_pick_action;		// whether picking action is started
	double dydOx;				// How much movement on the Ox corresponds with moving the targeted blob in x/y
	bool action_success = false;
	int action_success_user = -1;	// User's keypress of whether action was successful or not

	bool moving_pick_to_start = false;
	bool pick_at_start_pos = false; // Indicates whether the pick is moved to start position and is therefore ready to lower down
	bool cap_sensor_ready = false; // Indicates if the capacitive sensor has been power cycled and is ready to read for touch
	bool heating_pick = false; // Indicates if we are currently heating the pick.
	
	LidHandler lid_handler;		// Manages lid handling

	// Debug:
	cv::Point real_pt; /// for Debug: real point that the pick move to
	cv::Point adj_pt; /// for Debug: the coorected position that the pick move to (after adding an adjustment term calculated from the history)

	cv::Rect	Pick_ROI;
	double pick_focus_height_offset; // The height to which we move the pick above the focus height when readying for a pick action. So we expect we must lower this amount to touch the agar.


protected:

	// Pick movement calibration
	bool addPickPoint(int servo_id, int servo_rel_mvmt, std::vector<cv::Point>& tip_locs);

	// Pick tip to servo calibration
	double dxd1;		/// Amount of x-shift (pixels) for event increase of one dynamixel rotation unit on servo 1
	double dyd2;		/// Amount of Y-shift (pixels) for event increase of one dynamixel rotation unit on servo 2

	// Current status variables
	bool continue_lowering;			/// Start / continue lowering the pick to the stage
	bool abort_lowering;			/// Stop lowering and don't try agin. 
	bool continue_centering;		/// Start / continue finding the pick center
	bool finished_centering;
	bool start_test;								
	int tries;
	int servo2offset;
	int servo2offsetlimit;
	int servo2increment;
	int resetpos;

	// Sub-object that control pick motors
	DynamixelController Dxl;		/// Controller object for the dynamixels that man the pick
	ActuonixController Actx;		/// Controller object for the linear actuator. This raises and lowers the pick
	

	// Parameters for the pick
	std::vector<double> OxCenterPos;		/// Center position for the Grbl2
	double pick_thresh_rel;			/// Threshold used for the pick
	int pick_center_3;				/// Coordinate of servo 3 that positions the pick at the center of the field of view
	int pick_center_2;				/// Coordinate of servo 2 that positions the pick at the center of the field of view
	int pick_start_1;				/// Z position (servo 1) to start at when searching for pick center
	int grbl_center_y;
	int grbl_center_z;
	double lower_increment;			/// Step size when lowering Z2
	double lower_direction;		/// +1 (default, downwards) or -1;
	double detected_thresh;
	double focus_height;		/// Height of Z2 where tip is in focus / and/or touching the agar
	

	// Data about the pick
	std::vector<cv::Point> pick_coords;	/// Coordinates of all pick points
	
	cv::Point pick_tip, pick_tip_last;	/// Tip of the pick, if visible, and image center. 
	cv::Point2d pick_tip_rel;			/// Tip of pick also with relative to image units

	cv::Point pick_tip_conv, pick_tip_last_conv; /// Tip of the pick, computed by conventional machine vision methods
	cv::Point2d pick_tip_rel_conv;			/// Tip of pick also with relative to image units, computed by conventional machine vision methods

	int img_h; /// Height of image
	int img_w; /// Width of image

	/// Calibration of pick: mapping the servo coordinates with the tip position in FOV
	std::vector<cv::Point3i> calibr_servo_coords; /// Coordinates of servo 1-3, stored as 3D point x,y,z
	std::vector<cv::Point> calibr_tip_coords; /// Coordinates of pick tip
	std::vector<std::pair<cv::Point, cv::Point3i>> tip_servo_map; /// Mapping of pairs of above two
	int min_x; int min_y; int max_x; int max_y; /// Extreme values of calibrated region, i.e. boundary
	cv::Point target_pt; /// The target point that the pick wants to move to.
	
	int servo1_plate_height = 2276; /// ALERT - THIS VALUE MUST BE CALIBRATED ON A PER HARDWARE SET UP BASIS.

	double pick_gradient,				/// Current and previous gradient of pick edges, measured from last 5 points, to help with focusing
		pick_local_intensity,	/// Pick gradient computed using raw / unfiltered image
		pick_gradient_last,
		pick_gradient_diff,
		pick_gradient_diff_thresh,	/// Gradient value of pick tip must increase by less than this to be considered "in focus"
		pick_focus_gradient;		/// Minimum acceptable gradient of the pick when it is in focus
	std::deque <double> pick_gradient_history;	/// Keeps track of changes in the pick gradient
	std::deque <double> pick_local_max_history;  /// Keeps track of local max changes around pick (for impact detection)
	std::vector<double> focus_tenv;		/// Similar to pick_gradient_history but updated by lower pick routine
	std::vector<double> focus_mean;		/// Similar to focus_tenv
	std::vector<double> focus_AIN;		/// Stores the AIN value at each focus step (input will be 4-5 when contact occurs). 
	double pick_local_max_change;	/// Max difference in the local max history
	int pickup_angle;				/// Angle on servo 1 used to touch agar surface when operating blindly
	double intensity;				/// Mean intensity of pick itself (should be pretty dark)
	int pick_visible_frames;		/// Number of frames pick tip has been continuously visible
	double AIN0;					/// Value of LabJack AIN0, >=5V if contact occurs

	// Status of the pick
	bool pick_tip_visible;			/// Whether the pick tip is visible
	bool	pick_tip_down;			/// Whether the pick is currently at the focus plane
	int		pick_tip_centered_y;	/// Whether the pick is currently centered (0), below center (+1) or above center (-1)
	int		pick_tip_centered_x;	/// Whether the pick is currently centered (0), below cebter (+1) or above center (-1)
	bool	pick_tip_centered;		/// Whether the pick is centered or not
	double	pick_tip_dist_to_center;/// Distance of the pick_tip to the center, in units of FRACTION IMAGE WIDTH
	bool	pick_detected;
	bool	pick_tip_at_X_pickup;	/// Whether the pick tip has reached the pickup point (in X)
	bool	pick_tip_at_Y_pickup;	/// Whether the pick tip has reached the pickup point (in Y)
	bool	pick_tip_in_bounds;		/// Whether the pick tip is reasonably distant from the edges (assuming it is visisble)
	int		fail_count;
	bool	start_reading_for_touch;/// Whether to read AIN0 values from the labjack to determine whether touch has occurred
	HardwareSelector &use;			/// Whether to use pick components like GRBL or DXL
	ErrorNotifier& Err;
};

#endif