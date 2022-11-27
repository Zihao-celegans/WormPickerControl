/*
	WormPick.h
	Anthony Fouad
	Fang-Yen Lab
	April 2018

	Object containing information about the worm pick
*/

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
#include "opencv2\imgproc.hpp"

// Anthony's includes
#include "AnthonysTimer.h"
#include "AnthonysCalculations.h"
#include "DynamixelController.h"
#include "AnthonysKeyCodes.h"
#include "OxCnc.h"
#include "SortedBlobs.h"
#include "ThreadFuncs.h"
#include "ImageFuncs.h"


// Namespaces
using namespace std;

class WormPick {

	// Declare friend functions
	friend int PickWormScript(SortedBlobs *worms, OxCnc *Ox, OxCnc *Grbl2, WormPick *Pk);

public:

	// Constructor
	WormPick();						/// Blank pick object with NO intialized dynamixel
	WormPick(string fDynParams);	/// Usable pick object WITH initialized dynamixel

	// Destructor
	~WormPick();

	// Segment the pick
	bool segmentPick(const cv::Mat &imggray, const SortedBlobs &worms, cv::Mat& imgfilt);

	// Worm pick up
	bool pickWorm(OxCnc *Grbl2);

	// Worm-removal cycle - drag the pick around
	bool removeWorm(OxCnc *Grbl2);

	// Tweak the centering of the pick
	bool fineTuneCentering();

	// THREAD FUNCTION: Lower pick to focal plane when signalled, store pickup location in worms
	void lowerPickToFocus(OxCnc *Grbl2);

	// THREAD FUNCTION: Find the pick 
	void centerPick(OxCnc *Ox, OxCnc *Grbl2, SortedBlobs *worms);

	// THREAD FUNCTION: Test lowering-camera offset
	void testCameraOffset(OxCnc *Grbl2);



	// Getter 
	bool pickTipVisible() const;					/// Whether pick tip is visible
	bool pickTipDown() const;						/// Whether pick tip is down
	vector <cv::Point> getPickCoords() const;		/// List of coordinates of the pick
	cv::Point getPickTip() const;					/// Coordinates of the pick tip
	int getServoPos(int id);						/// Low level, Return Position of servo "id" or return -1 if none match id.
	int getPickStart1();							
	int getPickCenter2();
	int getPickCenter3();
	int getPickupAngle();
	int getGrblCenterY();
	int getGrblCenterZ();
	bool getFinishedCenter();
	int getIntensity();
	double getPickGradient(bool filtered_flag = true) const;
	double getLocalMaxChange() const;
	bool getTenvAlarmStatus() const;

	// Setter
	void resetIntensity();
	void movePickByKey(int &keynum, OxCnc *Grbl2);
	bool moveServo(int id, int pos, bool relflag, bool blockflag = false, bool suppressOOBwarn = false);  /// Low level, move a specific servo to a 
																			   /// specific position (relative or absolute) and 
																			   /// optionally suppress the out-of-bounds warning
	bool resetServo(int id);								/// Send a servo to its maximum
	bool resetServoToMin(int id);
	bool movePickToStartPos(OxCnc *Grbl2, bool move_grbl = true); /// Move the pick to the position needed to start lowering the pick
	bool movePickAway();									/// Move pick out of the way
	bool movePickSlightlyAway();							/// move pick just a little bit out of the way
	void startLoweringPick();								/// Send signal to lower pick
	void startRaisingPick();								/// Send signal to raise pick using same algorithm as lowering
	void startCenteringPick();								/// Send signal to center pick
	void setLoweringType(int type);							/// Change the current lowering style (see below for details)
	void setPickCenter3();									/// Set the current position of motor 2 to be the center
	void setPickCenter3(int setpos);						/// Set the specified angle to be motor 2 start position
	void setPickCenter(int s1, int s2, int s3, int gy, int gz);				/// Set the "Center" motor positions for the pick
	void setFinishedCenter(bool finished) { finished_centering = finished; }
																/// servo 1 - the start position for lowering
																/// servo 2,3 - Positions of servos 2 & 3 that center the pick

	void setPickStartServo1(int setpos);					/// Set the specified angle to be the motor 1 start position 
															///	for lowering 
	void setPickupAngle(int setpos);

protected:

	// Current status variables
	bool continue_lowering;			/// Start / continue lowering the pick to the stage
	bool continue_centering;		/// Start / continue finding the pick center
	bool finished_centering;
	bool finished_lowering;
	bool start_test;
	int tries;


	// Sub-object that control pick motors
	DynamixelController Dxl;		/// Controller object for the dynamixels that man the pick

	// Parameters for the pick
	vector<double> OxCenterPos;		/// Center position for the Grbl2
	double pick_thresh_rel;			/// Threshold used for the pick
	int pick_center_3;				/// Coordinate of servo 3 that positions the pick at the center of the field of view
	int pick_center_2;				/// Coordinate of servo 2 that positions the pick at the center of the field of view
	int pick_start_1;				/// Z position (servo 1) to start at when searching for pick center
	int grbl_center_y;
	int grbl_center_z;
	int lowering_type;					/// Style to use for lowering pick [DEPRECATED]
	double lower_increment;			/// Step size when lowering Z2
	double tenv_diff_alarm_thresh;  /// TENV differences greater than this value trigger an alarm
	bool tenv_diff_alarm;			/// TENV alarm status yes/no
	double tenv_diff_alarm_time;	/// TENV alarm time to stay on after any trigger
	AnthonysTimer tenv_alarm_timer; /// If TENV alarm is triggered, start the timer to leave it triggered for 3s
	double lower_direction;		/// +1 (default, downwards) or -1;
	double detected_thresh;		/// Threshold that says the pick has passed under the light
	int servo2offset;
	int servo2offsetlimit;
	int servo2increment; 
	int resetpos; 

	// Data about the pick
	vector<cv::Point> pick_coords;	/// Coordinates of all pick points
	cv::Point pick_tip, pick_tip_last;	/// Tip of the pick, if visible, and image center
	double pick_gradient,				/// Current and previous gradient of pick edges, measured from last 5 points, to help with focusing
		pick_local_intensity,	/// Pick gradient computed using raw / unfiltered image
		pick_gradient_last,
		pick_gradient_diff,
		pick_gradient_diff_thresh,	/// Gradient value of pick tip must increase by less than this to be considered "in focus"
		pick_focus_gradient;		/// Minimum acceptable gradient of the pick when it is in focus
	deque <double> pick_gradient_history;	/// Keeps track of changes in the pick gradient
	deque <double> pick_local_max_history;  /// Keeps track of local max changes around pick (for impact detection)
	vector<double> focus_tenv;		/// Similar to pick_gradient_history but updated by lower pick routine
	vector<double> focus_mean;		/// Similar to focus_tenv
	vector<double> avg_intensity	/// Stores the history of intensities
	double pick_local_max_change;	/// Max difference in the local max history
	int pickup_angle;				/// Angle on servo 1 used to touch agar surface when operating blindly
	int pick_visible_frames;		/// Number of frames pick tip has been continuously visible

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
};

#endif