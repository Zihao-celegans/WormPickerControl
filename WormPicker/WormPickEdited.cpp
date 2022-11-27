/*
	WormPick.cpp
	Anthony Fouad
	Fang-Yen Lab
	April 2018
*/

#include "WormPick.h"

// Anthony's includes
#include "AnthonysCalculations.h"

// OpenCV includes
using namespace cv;

// Contructor (blank object)
WormPick::WormPick() {
	pick_tip = Point(0, 0);
	pick_tip_visible = false;
	pick_detected = false;
	pick_tip_down = false;
	pick_tip_at_X_pickup = false;
	pick_tip_at_Y_pickup = false;
	pick_focus_gradient = 0;
	pick_thresh_rel = 0;
	pick_gradient = 0;
	continue_lowering = false;
	continue_centering = false;
	tries = 0;
	lowering_type = 0;
	servo2offset = 0;
	servo2offsetlimit = 400;
	servo2increment = 10;
	resetpos = 0;

	// Hardcoded parameters to be soft coded later
	pick_gradient_diff_thresh = 0;
}

// Constructor (real object)
WormPick::WormPick(string fDynParams) : Dxl(fDynParams) {
	pick_tip = Point(0, 0);
	pick_tip_last = Point(0,0);
	pick_tip_visible = false;
	pick_detected = false;
	pick_tip_down = false;
	pick_tip_at_X_pickup = false;
	pick_tip_at_Y_pickup = false;
	pick_thresh_rel = 0.9;
	pick_gradient = 0;
	continue_lowering = false;
	continue_centering = false;
	start_test = false;
	tries = 0;
	lowering_type = 1;
	pick_local_max_change = 0;
	servo2offset = 0;
	servo2offsetlimit = 400;
	servo2increment = 10;
	resetpos = 0;

	for (int i = 0; i<3; i++)
		pick_gradient_history.push_back(0);

	for (int i = 0; i < 20; i++)
		pick_local_max_history.push_back(0);

	// Hardcoded parameters to be soft coded later
	pick_center_3 = 1140;
	pick_center_2 = 1140;
	pick_start_1 = 1140;
	pick_gradient_diff_thresh = -0.1;	/// If Gradient does not increase by at least this much,
										///		we're no longer getting in better focus
										///		(.e.g as long as it does NOT decrease)
	pick_focus_gradient = 2;			/// Intentionally loose, Gradient used for finding focus must be above this value
	lower_increment = 1;
	tenv_diff_alarm_thresh = 99; /// basically disabled for now, actual value would be 1-5 
	tenv_diff_alarm_time = 1.5; /// Seconds
	tenv_alarm_timer.setToZero();

	// Move the pick clear of the plate
	movePickAway();
}

// Destructor
WormPick::~WormPick() {
}


/* 
	Analyze the image to segment the pick - VERSION 7/5/2018 - EDGE OF UPPER AND LOWER USED 
	https://docs.opencv.org/2.4/doc/tutorials/imgproc/imgtrans/sobel_derivatives/sobel_derivatives.html

*/

bool WormPick::segmentPick(const Mat &imggray, const SortedBlobs &worms, Mat& imgfilt) {


	// Get upper and lower boundaries of the pick edges

	/// Declare and initialize variables

	// Basic variables
	Size	sz = imggray.size();
	int		w = sz.width;
	int		h = sz.height;
	int		col_interval = 3;
	double	minVal = 0;
	Point	minLoc = Point(0, 0);
	int min_pick_points = 0.25*w/col_interval;
	
	// Apply substantial smoothing to the image to filter out worms and other noise
	GaussianBlur(imggray, imgfilt, Size(41, 41), 31, 31);

	// Find the absolute minimum in each column
	pick_coords.clear();
	for (int i = 0; i < w; i += col_interval) {

		/// Set ROI to this column
		Rect r(i, 0, 1, h);
		Mat col_img = imgfilt(r);

		/// Get minimum and location
		minMaxLoc(col_img, &minVal, NULL, &minLoc, NULL);

		/// Add to the list
		pick_coords.push_back(Point(i, minLoc.y));
	}

	// Determine average intensity of the image to detect whether pick has passed under the light
	avg_intensity.push_back(mean(imggray, w, h, w, h))
	detected_thresh = 5.0;
	if (avg_intensity.size() > 0) {
		double last_avg = avg_intensity[avg_intensity.size() - 2];
		double current_avg = avg_intensity[avg_intensity.size() - 1];
		if (current_avg - last_avg > detected_thresh) {
			pick_detected = true;
		}
	}


	// Remove pick points that are "stuck" to the top or bottom
	for (int i = pick_coords.size()-1; i >= 0 ; --i) {
		if (pick_coords[i].y < 5 || pick_coords[i].y > h - 5) {
			pick_coords.erase(pick_coords.begin() + i);
		}
	} 

	// Find longest continuous segments
	double continuity_thresh = 0.09;
	int max_y_jump = continuity_thresh * h;
	int max_x_jump = col_interval * 2;
	isolate_longest_chain_by_y_jump(pick_coords, max_x_jump, max_y_jump, min_pick_points);

	// Identify pick tip
	if (pick_coords.size() > 0) {
		pick_tip_last = pick_tip;
		pick_tip = Point(pick_coords[pick_coords.size() - 1]);
	}
	else {
		pick_tip = Point(0, 0);
		pick_tip_last = Point(w, h / 2);
	}

	// pick_tip must be at least 5% away from the right bound and left bound to be valid
	if (pick_tip.x > w - 0.05*w || pick_tip.x < 0.05*w) {
		pick_tip = Point(0, 0);
		pick_tip_visible = false;
	}
	else {
		pick_tip_visible = true;
	}

	// Pick must be long enough and close to the left boundary to be valid
	if (pick_coords.size() < min_pick_points || pick_coords[0].x > 0.2*w) {
		pick_tip_visible = false;
		pick_coords.clear();
	}
		
	// Trim pick tip to alleviate blur artefact
	if (pick_tip_visible) {
		/// Establish search limit (5% of the points)
		int max_trim = 0.1*pick_coords.size();
		int start_idx = pick_coords.size() - max_trim;
		int end_idx = pick_coords.size() - 1;

		/// Find the darkness values within the pick and at the tip
		int ref_dark_idx = pick_coords.size() - 0.15*pick_coords.size();
		double ref_dark = imggray.at<uchar>(pick_coords[ref_dark_idx]);
		double ref_tip = imggray.at<uchar>(pick_tip);

		/// Walk backwards along edge until we ready 67% of the darkness
		double cutoff_dark_frac = 0.67;
		double cutoff_drop = cutoff_dark_frac*(ref_tip - ref_dark), current_diff = 0;
		int new_tip_idx = pick_coords.size() - 1;

		for (new_tip_idx = end_idx; new_tip_idx > start_idx; new_tip_idx--) {
			double current_drop = ref_tip - imggray.at<uchar>(pick_coords[new_tip_idx]);
			if (current_drop > cutoff_drop)
				break;
		}

		/// Purge the last few points from the pick
		for (int i = end_idx; i > new_tip_idx; i--) {
			pick_coords.pop_back();
		}

		/// Update the tip
		pick_tip = pick_coords[new_tip_idx];
	}

	// Measure Y gradient as tenengrad variance (TENV), near tip if possible, otherwise in whole image
	int buffer = 0.05*h;
	int buffer_x = 0.05*w;
	int tenv_radius = 0.12*w; 

	if (pick_tip_visible) {
		pick_gradient = tenengrad(imgfilt, pick_tip, tenv_radius);
		pick_local_intensity = localIntensity(imgfilt, pick_tip, tenv_radius);
	}
	else {
		pick_gradient = tenengrad(imgfilt, pick_tip_last, tenv_radius);
		pick_local_intensity = localIntensity(imgfilt, pick_tip_last, tenv_radius);
	}

	// Add the pick radius to a history for the last 3 frames for less noisy radius measurement
	/// Add the current value to the end
	pick_gradient_history.push_back(pick_gradient);

	/// Remove oldest value from beginning
	pick_gradient_history.pop_front();

	/// Average to get the true pick gradient
	double sum_of_deque = 0;
	double num_in_deque = 0;
	for (int n = 0; n < pick_gradient_history.size(); n++) {
		sum_of_deque += pick_gradient_history[n];
		num_in_deque++;
	}
	pick_gradient = sum_of_deque / num_in_deque;

	// Determine whether the visible pick tip is in focus
	/// NOTE: focus improvement tracking (e.g. radius_diff) is updated by the lowerPickToFocus() routine.
	pick_tip_down = pick_tip_visible && (pick_gradient > pick_focus_gradient) && (pick_gradient_diff < pick_gradient_diff_thresh);

	// Determine whether the pick tip has reached the pickup location in X
	pick_tip_at_X_pickup = pick_tip_visible && pick_tip.x <= worms.pickup_location.x;
	pick_tip_at_Y_pickup = pick_tip_visible && abs(pick_tip.y - worms.pickup_location.y) < h / 20;

	// Determine whether the pick is centered in the middle fifth of this image (regardless of whether it is down)
	
	/// Measure distance to center if needed elsewhere in units of FRACTION of WIDTH
	pick_tip_dist_to_center = ptDist(pick_tip, Point(w / 2.0, h / 2.0)) / w;

	/// If pick is not visible, then leave the value unchanged
	buffer = 0.025*h;

	/// Center
	if (pick_tip_visible && (pick_tip.y > h / 2.0 - buffer) && (pick_tip.y < h / 2.0 + buffer))
		pick_tip_centered_y = 0;

	/// Below center
	else if (pick_tip_visible && (pick_tip.y >= h / 2.0 + buffer))
		pick_tip_centered_y = 1;

	/// Above center
	else if (pick_tip_visible && (pick_tip.y <= h / 2.0 - buffer))
		pick_tip_centered_y = -1;

	/// Center
	if (pick_tip_visible && (pick_tip.x > w / 2.0 - buffer) && (pick_tip.x < w / 2.0 + buffer))
		pick_tip_centered_x = 0;

	/// Right of center
	else if (pick_tip_visible && (pick_tip.x >= w / 2.0 + buffer))
		pick_tip_centered_x = 1;

	/// Left of center
	else if (pick_tip_visible && (pick_tip.x <= w / 2.0 - buffer))
		pick_tip_centered_x = -1;
	
	if (pick_tip_centered_x == 0 && pick_tip_centered_y == 0)
		pick_tip_centered = true;
	else
		pick_tip_centered = false;


	// Determine whether an impact has occurred (abrupt change in TENV)
	///		* This is done on EVERY frame, not just at lowering steps, but only while lowering is active.
	///		* If the TENV alarm goes off, it stays on for 3 second to allow detection
	///			by the focus lowering routine before it resets.
	///		* TENV alarm is only checked when the pick tip is visible AND has been visible for at least 2 frames
	///			(there would be a sudden TENV change between whole image and ROI tenv)
	int L = pick_gradient_history.size() - 1;
	double tenv_diff = abs(pick_gradient_history[L] - pick_gradient_history[L - 1]);


	/// TENV alarm is on AND it has been longer than the time limit
	if (tenv_alarm_timer.getElapsedTime() > tenv_diff_alarm_time && tenv_diff_alarm) {
		printf("TENV ALARM FINISHED!\n");
		tenv_diff_alarm = false;
	}

	/// TENV alarm is on and should continue to be on until the time limit regardless of whether the tip is visible
	else if (tenv_diff_alarm) {
	}

	/// TENV alarm is NOT on, but should be triggered on this frame
	else if (pick_tip_visible && continue_lowering && tenv_diff > tenv_diff_alarm_thresh) {
		tenv_diff_alarm = true;
		printf("TENV ALARM START - diff=%0.2f - %0.2f\n", tenv_diff , pick_gradient_history[L] , pick_gradient_history[L-1]);
		tenv_alarm_timer.setToZero();
	}

	/// TENV alarm is neither on nor should be on
	else {
		tenv_diff_alarm = false;
	}


	return 0;

}

bool WormPick::pickWorm(OxCnc* Grbl2) {
	
	// Set pick to the start position
	movePickToStartPos(Grbl2);

	// Lower pick to worm
	startLoweringPick();

	// Wait for the worm to get picked
	while (continue_lowering) { boost_sleep_ms(25); }

	// Sweep pick to "scoop" worm
	//Dxl.moveServo(2, -5, true, false, true);
	//boost_sleep_ms(1000);
	//Dxl.moveServo(2, 5, true, false, true);


	return true;
}


// Worm-removal cycle - drag the pick around. Return value indicates whether worm was dislodged successfully
bool WormPick::removeWorm(OxCnc *Grbl2) {

	// Check whether the pick is appropriately lowered, if not do so now
	AnthonysTimer Ta;
	if (!(pick_tip_down || pick_tip_at_X_pickup)) {
		//setLoweringType(2);
		//startLoweringPick();
		while (!pick_tip_down && Ta.getElapsedTime() < 8) { boost_sleep_ms(25); }
		if (Ta.getElapsedTime() > 8) { cout << "WARNING: Failed to lower pick during removal cycle\n"; }
	}

	// Try up to 5 times to dislodge the worm
	int iter = 0, max_iters = 1;

	while (true) {

		// drag pick in Y
		for (int i = 0; i < 25; i++) { moveServo(2, 1, true, true); boost_sleep_ms(50); }

		// Raise pick and move it out of the way
		movePickAway();

		// Check whether worm was dropped
		if (false) { ////////////////// CHECK WHETHER WORM IS VISIBLE NEAR DROPOFF SITE
			return true;
		}

		// Return false if we tried 5 times and failed
		else if (iter++ >= max_iters) {
			return false;
		}  

		// If worm not dropped, lower pick back and wait for it to be lowered so we can try again
		pickWorm(Grbl2);
		Ta.setToZero();
		while (!pick_tip_down && Ta.getElapsedTime() < 8) { boost_sleep_ms(25); }
		if (Ta.getElapsedTime() > 8) { cout << "WARNING: Failed to lower pick during removal cycle\n"; }
	}

	// If we failed to dislodge the worm return false
	return false;
}

// Fine tune pick centering (1 step only in each axis)
bool WormPick::fineTuneCentering() {

	// Step in Y direction towards center using DXL motor 2
	if (!(pick_tip_centered_y == 0) && pick_tip_visible) {
		if (pick_tip_centered_y == 1) { Dxl.moveServo(2, 1, DXL_RELATIVE, false, true); }
		else { Dxl.moveServo(2, -1, true, false, true); }
		boost_sleep_ms(50);
	}

	// Step in X direction towards center using DXL motor 1
	if (!(pick_tip_centered_x == 0) && pick_tip_visible) {
		if (pick_tip_centered_x == 1) { Dxl.moveServo(1, 1, DXL_RELATIVE, false, true); }
		else { Dxl.moveServo(1, -1, true, false, true); }
		boost_sleep_ms(50);
	}

	return true;
}

// Lower pick to focal plane OR sudden intensity drop -- thread function
void WormPick::lowerPickToFocus(OxCnc *Grbl2) {
	
	for (;;) {

		// Check whether a TENV alarm (e.g. hit surface) is activated, if so cancel lowering
		if (tenv_diff_alarm) {
			continue_lowering = false;
		}

		// Check whether lowering was signalled, if not do nothing and clear the focus curve
		boost_interrupt_point();
		if (!continue_lowering) {
			focus_tenv.clear();
			focus_mean.clear();
			fail_count = 0;
		}

		// If signalled, but before lowering, enforce pick centering. Pause to re-center if needed.
		/*while (continue_lowering && !pick_tip_centered) {
			fineTuneCentering();
			boost_sleep_ms(10);
			boost_interrupt_point();
		}*/

		// If signalled, lower pick by 1 degree until in focus
		if (continue_lowering && pick_tip_visible) {

			/// If we're just starting the lowering, then seed the focus curve with the current tenengrad
			if (focus_tenv.size() == 0) {
				focus_tenv.push_back(pick_gradient);
				focus_mean.push_back(pick_local_intensity);
			}

			/// Move Down unless we've reached the limit of movement
			if (!Grbl2->goToRelative(0, 0, lower_increment*lower_direction)) {
				cout << "Reached limit while lowering pick...\n";
				continue_lowering = false;
				continue; 
			}
			boost_sleep_ms(350);

			/// Record Pick focus metric locally until it decreases. If it seems to decrease, 
			///		wait a moment and triple check to confirm it wasn't a movement / vibration artefact.
			///		*if TENV decreased, waiting allows us to confirm it wasn't noise
			///		*if local intensity decreases suddenly, it should actually get smaller and smaller on each subsequent check
			int tries = 0;
			double focus_mean_diff_stop = -3, focus_mean_diff_slow = -1.5;
			double pick_intensity_diff = 0;

			while ((pick_gradient <= focus_tenv[focus_tenv.size() - 1] && tries < 3) ||
				(pick_local_intensity - focus_mean[focus_tenv.size() - 1] < focus_mean_diff_stop && tries < 3))
			{
				boost_sleep_ms(150 * tries);
				printf("Checking for stop: %0.2f\t%0.2f\n", pick_gradient, pick_local_intensity);
				tries += 1;
			}

			/// Slow down if local intensity is decreasing in case we have hit agar
			if (pick_local_intensity - focus_mean[focus_tenv.size() - 1] < focus_mean_diff_slow)
				boost_sleep_ms(250);

			focus_tenv.push_back(pick_gradient);
			focus_mean.push_back(pick_local_intensity);
			int idx = focus_tenv.size()-1;
			pick_gradient_diff = focus_tenv[idx] - focus_tenv[idx - 1];
			pick_intensity_diff = focus_mean[idx] - focus_mean[0];

			///// DEBUG: Display focus curve
			printf("----------------------------------------\n");
			for (int i = 0; i<focus_tenv.size(); i++)
				printf("%0.2f\t%0.2f\n", focus_tenv[i],focus_mean[i]);
			printf("\n");

			/// Determine whether to continue lowering or not
			continue_lowering = pick_gradient_diff > pick_gradient_diff_thresh && pick_intensity_diff > focus_mean_diff_stop;
		}

		// If we've lost track of the pick and tried 5 times to re-find it, then quit
		else if (continue_lowering && !pick_tip_visible && fail_count >= 5) {
			cout << "Pick is not visible, cannot lower by GRBL2!\n";
			continue_lowering = false;
		}

		// If we've lost track of the pick, then try again (up to 5 times)
		else if (continue_lowering && !pick_tip_visible) {
			fail_count += 1;
			boost_sleep_ms(100);
		}

		boost_sleep_ms(50);
		boost_interrupt_point();
	}
}


// Center the pick -- thread function
void WormPick::centerPick(OxCnc *Ox, OxCnc *Grbl2, SortedBlobs *worms) {
	for (;;) {
		if (continue_centering) {
			
			/// Raise gantry regardless of case
			if (tries == 0)
				Ox->raiseGantry();

			/// Case 1 (see below): Tip is visible, simply fine tune it from present position. Don't raise or reset.

			/// Case 2: Tip is not visible. Try move back to previous start location and see if it's visible. 
			if (tries == 0 && !pick_tip_visible) {
				while(Ox->isMoving()){}
				movePickToStartPos(Grbl2);  /// Move pick to last known location
				boost_sleep_ms(100); /// Pause for a sec (in addition to waiting for movement to complete)
			}

			/// Case 3: Tip was not visible in either case. Start from scratch. 
			if (!pick_tip_visible) {

				/// If the pick tip isn't visible and it is the first try, reset to a position
				if (tries == 0) {
					Grbl2->goToAbsolute(Grbl2->getX(), -2, -115,true);
					resetServo(3);
					resetServoToMin(1);
				}

				/// Decrease Y position until failure OR pick is detected in the main thread
				while (Dxl.moveServo(2, servo2increment, true, false, true) && continue_centering && servo2offset < servo2offsetlimit) {
					servo2offset = resetpos + servo2increment; /// Keep track of how much we've moved
					if (pick_detected) {
						resetpos = servo2offset - 20;
						servo2offsetlimit = servo2offset + 20;
						servo2increment = 5;
						boost_sleep(250);
					}
					else { boost_sleep}
					if (pick_tip_visible) { break; }
					boost_interrupt_point();
					boost_sleep_ms(15);
				}
				servo2offset = 0;

				/// If pick tip is not visible, move DXL motor 1 and Grbl2 Y (X3)
				if (!pick_tip_visible) {
					if (pick_detected) {
						Dxl.moveServo(2, resetpos, false, false, true);

						if (!Dxl.moveServo(1, 14, true, false, true)) {
							cout << "WARNING: Failed to find the pick. Resetting...\n";
							Dxl.resetServo(1);
							continue_centering = false;
							continue;
						}
					}
					else {
						Grbl2.goToRelative(0, -5, 0);
						Dxl.resetServoToMin(2);
					}
				}
			}

			/// If pick tip is centered, proceed with final cleanup 
			if (pick_tip_centered_x == 0 && pick_tip_centered_y == 0 && pick_tip_visible) {

				/// If it is, raise the pick up first (coarsely) to make sure we aren't below the focus plane
				startRaisingPick();
				double l_i_bak = lower_increment;
				lower_increment = 1;
				while (continue_lowering) { boost_sleep_ms(25); } 

				/// Lower pick to focus plane (coarsely) and re-center before assigning pickup location
				boost_sleep_ms(500);
				startLoweringPick();
				while (continue_lowering) { boost_sleep_ms(25); }
				for(int i = 0; i<5; i++){ 
					fineTuneCentering(); 
					boost_sleep_ms(250);
				}
				lower_increment = l_i_bak;

				/// Assign pickup location using the in-focus tip
				worms->pickup_location = pick_tip;

				/// Raise pick so it won't hit ground when lowering. If position is OOB, keep trying smaller intervals until it's in
				for (int i = -20; i < 0; i++) {
					if (Grbl2->goToRelative(0, 0, i, true));
					break;
				}

				/// Once raised above focal plane, save the start_lowering positions
				boost_sleep_ms(400);
				setPickCenter(Dxl.positionOfServo(1), Dxl.positionOfServo(2), Dxl.positionOfServo(3), Grbl2->getY(), Grbl2->getZ());

				/// Lower the Gantry back to its original level and reset
				Ox->lowerGantry();			 
				cout << "Centering pick done\n";
				continue_centering = false;
			}

			/// Tune x and y centering
			else {
				fineTuneCentering();
			}
			tries += 1;
		}
		/// Reset tries if continue centering is false
		else {
			tries = 0;
		}
		boost_interrupt_point();
	}
}

// Checks the offset of the camera is aligned with Z axis
void WormPick::testCameraOffset(OxCnc *Grbl2) {
	for (;;) {
		if (start_test && pick_tip_visible) {
			int currentx = pick_tip.x;
			int currenty = pick_tip.y;
			Grbl2->goToRelative(0, 0, 20, 1.0, true, 5);
			cout << "X diff: " << abs(pick_tip.x - currentx) << " Y diff: " << abs(pick_tip.y - currenty) << endl;
			Grbl2->goToRelative(0, 0, -20, 1.0, true, 5);
		}
		boost_sleep_ms(2000);
		boost_interrupt_point();
	}
}

// Move the pick by key press
void WormPick::movePickByKey(int &keynum, OxCnc *Grbl2) {

	// Check whether user requested any manual movement of the servos. If so, apply movement and halt any auto movements
	///		First, interrupt any in-progress thread operations, wait a moment to ensure they stopped, then move for real
	if (Dxl.moveServoByKey(keynum,true)) {
		if (continue_lowering || continue_centering) {
			continue_lowering = false;
			continue_centering = false;
			boost_sleep_ms(250);
		}
		Dxl.moveServoByKey(keynum);
	}

	// Check whether the user requested that the pick be lowered to the stage
	if (keynum == keyEnd) { 
		setLoweringType(1); 
		startLoweringPick(); 
		cout << "Lowering pick using FOCUS...\n"; 
		//start_test = true;
		keynum = -5; 
	}

	if (keynum == keyDelete) {
		pickWorm(Grbl2);
		//cout << "Lowering pick using PICKUP LOCATION...\n";
		keynum = -5;
	}

	// Check whether the user requested that the pick be centered now
	if (keynum == keyHome) { 
		startCenteringPick(); 
		cout << "Centering pick...\n"; 
		keynum = -5;
	}

	// Check whether the user requested that the pick be moved to (or away from) the start-pick position
	if (keynum == keyInsert) {
		if (!pick_tip_visible)
			movePickToStartPos(Grbl2);
		else
			movePickSlightlyAway();
		keynum = -5;
	}
}

// Low level function: Get position of servo "id"
int WormPick::getServoPos(int id) {
	for (int i = 0; i < Dxl.numberOfServos(); ++i) {
		return Dxl.positionOfServo(id);
	}
	return -1;
}

// Low level function: Move servo "id" to a specific position
bool WormPick::moveServo(int id, int pos, bool relflag, bool blockflag, bool suppressOOBwarn) {
	return Dxl.moveServo(id, pos, relflag, blockflag, suppressOOBwarn);
}

// Low level function: Move servo "id" to its maximum position
bool WormPick::resetServo(int id) { return Dxl.resetServo(id); }

// Low level function: Move servo "id" to its minimum position
bool WormPick::resetServoToMin(int id) { return Dxl.resetServoToMin(id); }

// Move the pick to the start-lowering position
bool WormPick::movePickToStartPos(OxCnc *Grbl2, bool move_grbl) { 

	/// Move Grbl2 motors to start position
	Grbl2->goToAbsolute(Grbl2->getX(), grbl_center_y, grbl_center_z,true);

	/// Move dynamixel motors to center position position while servo 1 is UP.
	Dxl.resetServoToMin(1);
	Dxl.moveServo(2, pick_center_2, false, false, false);
	Dxl.moveServo(3, pick_center_3, false, false, false);

	/// Last, lower pick to start position
	boost_sleep_ms(400);
	Dxl.moveServo(1, pick_start_1, false, false, false);
	boost_sleep_ms(400);

	return true;
}

// Move the pick out of the way
bool WormPick::movePickAway() {

	resetServoToMin(1);
	boost_sleep_ms(200);
	resetServo(2);
	boost_sleep_ms(200);
	resetServoToMin(3);	
	return true;
}

// Move the pick slightly out of the waya
bool WormPick::movePickSlightlyAway() {

	resetServoToMin(1);
	boost_sleep_ms(200);
	moveServo(3, -100, true);
	boost_sleep_ms(200);
	return true; 
}


// Getter
bool WormPick::pickTipVisible() const { return pick_tip_visible; }
bool WormPick::pickTipDown() const { return pick_tip_down; }
vector <cv::Point> WormPick::getPickCoords() const { return pick_coords; }
int WormPick::getPickStart1() { return pick_start_1; }
int WormPick::getPickCenter2() { return pick_center_2; }
int WormPick::getPickCenter3() { return pick_center_3; }
int WormPick::getPickupAngle() { return pickup_angle; }
int WormPick::getGrblCenterY() { return grbl_center_y; }
int WormPick::getGrblCenterZ() { return grbl_center_z; }
bool WormPick::getFinishedCenter() { return finished_centering; }
int WormPick::getIntensity() { return intensity;  }
cv::Point WormPick::getPickTip() const { return pick_tip; }
double WormPick::getPickGradient(bool filtered_flag) const { 
	if (filtered_flag)
		return pick_gradient;
	else
		return pick_local_intensity;
}
double WormPick::getLocalMaxChange() const { return pick_local_max_change; }
bool WormPick::getTenvAlarmStatus() const { return tenv_diff_alarm; }


// Setter
void WormPick::resetIntensity() { intensity = 0;  }
void WormPick::startLoweringPick() { lower_direction = 1; continue_lowering = true; }
void WormPick::startRaisingPick() { lower_direction = -1; continue_lowering = true; }
void WormPick::startCenteringPick() { continue_centering = true; }
void WormPick::setLoweringType(int type) { lowering_type = type; }
void WormPick::setPickCenter3() { pick_center_3 = Dxl.positionOfServo(3); }
void WormPick::setPickCenter3(int setpos) { pick_center_3 = setpos; }
void WormPick::setPickCenter(int s1, int s2, int s3, int gy, int gz) {
	pick_start_1 = s1;
	pick_center_2 = s2;
	pick_center_3 = s3;

	grbl_center_y = gy;
	grbl_center_z = gz;
}
void WormPick::setPickStartServo1(int setpos) { pick_start_1 = setpos; }
void WormPick::setPickupAngle(int setpos) { pickup_angle = setpos; }