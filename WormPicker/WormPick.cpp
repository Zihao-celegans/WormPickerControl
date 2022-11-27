/*
	WormPick.cpp
	Anthony Fouad
	Fang-Yen Lab
	April 2018
*/

#include "WormPick.h"

// Anthony's includes
#include "LabJackFuncs.h"
#include "ImageFuncs.h"
#include "WPHelper.h"
#include "Config.h"
#include "json.hpp"
#include "DnnHelper.h"

// Standard includes
#include <iostream>
#include <numeric>      /// std::adjacent_difference
#include <ctime>

// OpenCV includes
using namespace cv;
using namespace std;
using json = nlohmann::json;

// Contructor (blank object)
/*
WormPick::WormPick(bool &ipa, HardwareSelector _use, ErrorNotifier &_Err) :
	in_pick_action(ipa),
	use(_use),
	Dxl(_use),
	Err(_Err)
{
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
	servo2offset = 0;
	servo2offsetlimit = 400;
	servo2increment = 10;
	resetpos = 600;
	in_pick_action = false;

	// Hardcoded parameters to be soft coded later
	pick_gradient_diff_thresh = 0;
}*/

// Constructor (real object)
WormPick::WormPick(bool &ipa, HardwareSelector _use, ErrorNotifier &_Err) :
	Dxl(_use),
	in_pick_action(ipa),
	use(_use),
	Err(_Err),
	Actx(_use)
{
	
	pick_tip = Point(0, 0);
	pick_tip_last = Point(0,0);

	pick_tip_conv = Point(0, 0);
	pick_tip_last_conv = Point(0, 0);

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
	pick_local_max_change = 0;
	servo2offset = 0;
	servo2offsetlimit = 400;
	servo2increment = 10;
	resetpos = 600;
	in_pick_action = false;
	lower_direction = 1;
	pick_focus_height_offset = 250; // We measured linear actuator units to be ~100 units = 1mm. So 120 will be a 1.2mm offset above the focus.

	for (int i = 0; i<3; i++)
		pick_gradient_history.push_back(0);

	for (int i = 0; i < 20; i++)
		pick_local_max_history.push_back(0);

	// Load values for pick center
	loadPickupInfo();
	pick_gradient_diff_thresh = -0.15;	/// If Gradient does not increase by at least this much,
										///		we're no longer getting in better focus
										///		(.e.g as long as it does NOT decrease)
	pick_focus_gradient = 2;			/// Intentionally loose, Gradient used for finding focus must be above this value
	lower_increment = 0.5;

	// Move the pick clear of the plate
	movePickAway();
	Actx.homeActuonix();
	Actx.moveAbsolute(focus_height);
	start_reading_for_touch = false;

	// Set the pick target point to default
	target_pt.x = -1; target_pt.y = -1;

	// Debug:
	real_pt.x = -1; real_pt.y = -1;
	adj_pt.x = -1; adj_pt.y = -1;
	Pick_ROI.x = -1; Pick_ROI.y = -1;

	// Set the calibration boundary to default
	min_x = -1; min_y = -1; max_x = -1; max_y = -1;
	// Load calibration map from file
	CaliMapfromfile("C:\\WormPicker\\PickCalibration\\PickMap.txt");
}

// Destructor
WormPick::~WormPick() {

	savePickupInfo();
	movePickAway();

}

/*
	Load and save pickup location info
*/

Point WormPick::loadPickupInfo() {

	json config = Config::getInstance().getConfigTree("pickup");
	
	Point img_pickup_loc; // This parameter seems to be no longer in use
	img_pickup_loc.x = config["location"]["x"].get<int>(); // This parameter seems to be no longer in use
	img_pickup_loc.y = config["location"]["y"].get<int>(); // This parameter seems to be no longer in use
	pick_start_1 = config["start"].get<int>();
	pick_center_2 = config["center2"].get<int>();
	pick_center_3 = config["center3"].get<int>();
	pickup_angle = config["angle"].get<int>();
	grbl_center_y = config["grbl center"]["y"].get<int>(); // This parameter seems to be no longer in use
	grbl_center_z = config["grbl center"]["z"].get<int>(); // This parameter seems to be no longer in use
	focus_height = config["focus height"].get<double>();
	dydOx = config["dydOx"].get<double>();

	return img_pickup_loc;
}

void WormPick::savePickupInfo(Point img_pickup_pt) {

	Config::getInstance().loadFiles();

	json config = Config::getInstance().getConfigTree("pickup");
	config["location"]["x"] = img_pickup_pt.x;
	config["location"]["y"] = img_pickup_pt.y;
	config["start"] = pick_start_1;
	config["center2"] = pick_center_2;
	config["center3"] = pick_center_3;
	config["angle"] = pickup_angle;
	config["grbl center"]["y"] = grbl_center_y;
	config["grbl center"]["z"] = grbl_center_z;
	config["focus height"] = focus_height;
	config["dydOx"] = dydOx;
	Config::getInstance().replaceTree("pickup", config);
	Config::getInstance().writeConfig();
}

/// Variant for saving only the wormpick hardware info, not the image pickup point that usually doesn't chage
void WormPick::savePickupInfo() {

	json config = Config::getInstance().getConfigTree("pickup");
	Point img_pickup_pt;

	// Load the old image pickup point
	img_pickup_pt.x = config["location"]["x"].get<int>();
	img_pickup_pt.y = config["location"]["y"].get<int>();

	savePickupInfo(img_pickup_pt);
}


/* 
	Analyze the image to segment the pick - VERSION 7/5/2018 - EDGE OF UPPER AND LOWER USED 
	https://docs.opencv.org/2.4/doc/tutorials/imgproc/imgtrans/sobel_derivatives/
	_derivatives.html

*/

bool WormPick::segmentPick(const Mat &imggray, const WormFinder &worms, Mat& imgfilt) {


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

	// Keep track of how dark the pick is relative to the image average - should be much darker
	double img_avg = mean(imgfilt)[0];
	intensity = 0;

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

	// Calculate track the average intensity the pick
	for (int i = 0; i < pick_coords.size(); i++) 
		intensity += imgfilt.at<uchar>(pick_coords[i]);
	
	intensity /= pick_coords.size();


	// The average brightness of the pick points much be less than 2/3 the image average
	if (intensity > 0.73*img_avg) {
		pick_coords.clear();
	}

	// Identify pick tip
	if (pick_coords.size() > 0) {
		pick_tip_last = pick_tip;
		pick_tip = Point(pick_coords[pick_coords.size() - 1]);
		pick_tip_rel = Point2d((double) pick_tip.x / (double) w, (double) pick_tip.y / (double) h);
	}
	else {
		pick_tip = Point(0, 0);
		pick_tip_last = Point(w, h / 2);
		pick_tip_rel = Point(0, 0);
	}

	// pick_tip must be at least 5% away from the right bound and left bound to be valid
	if (pick_tip.x > w - 0.05*w || pick_tip.x < 0.05*w) {
		pick_tip = Point(0, 0);
		pick_tip_visible = false;
	}
	else {
		pick_tip_visible = true;
	}

	// Pick must be long enough and terminate to the left boundary to be valid
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

	// get information about the pick tip
	computePickTipInfo(imgfilt,worms,h,w);

	// Determine whether an impact has occurred (LabJack DAC0 capacative sensor)
	// AIN0 = LabJack_ReadAINN(lngHandle, 0);
	/// moved AIN0 read to separate thread function


	return 0;

}



bool WormPick::segmentLoop(const Mat &imggray, const WormFinder &worms, Mat& imgfilt) {


	// Basic variables
	Size	sz = imggray(Pick_ROI).size();
	int		w = sz.width;
	int		h = sz.height;
	int		col_interval = 3;
	double	minVal = 0;
	Point	minLoc = Point(0, 0);
	int min_pick_points = 0.25*w / col_interval;

	// Don't both segmenting dark images?

	// Apply substantial smoothing to the image to filter out worms and other noise
	GaussianBlur(imggray(Pick_ROI), imgfilt, Size(3, 3), 31, 31);

	//imshow("imgfilt", imgfilt);
	//waitKey(1);

	// Find largest very dark object
	Mat bw;
	//threshold(imgfilt, bw, 50, 255, THRESH_BINARY_INV); // orginal:50
	adaptiveThreshold(imgfilt, bw, 255, ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY_INV, 91, 5);

	//imshow("bw", bw);
	//waitKey(1);

	isolateLargestBlob(bw, 0);// original:30

	/*imshow("bw_iso", bw);
	waitKey(1);*/

	// Get contours of largest blob
	vector<vector<Point>> contours;
	findContours(bw, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

	// Downsample the contours to increase performance
	pick_coords.clear();
	if ((contours.size() >= 1)) {
		for (int i = 0; i < contours[0].size(); i+=3)
			pick_coords.push_back(contours[0][i]);
	}
	else { 
		clearPick();
		return false; 
	}


	// Extract all points that contact the left hand side. Also get the right-most point
	//vector<int> lhs_ys;
	Point rmost(0, 0);
	Point dowmost(0, 0);
	Point upmost(0, 0);
	for (int i = 0; i < pick_coords.size(); i++) {

		// Does this point touch the LHS?
	/*	if (pick_coords[i].x <= 30)
			lhs_ys.push_back(pick_coords[i].y);*/

		// Is this point towards the right (tip?)
		if (pick_coords[i].x > rmost.x)
			rmost = pick_coords[i];

		if (pick_coords[i].y > dowmost.y)
			dowmost = pick_coords[i];

		if (pick_coords[i].y < upmost.y)
			upmost = pick_coords[i];

	}

	// If the pick does not touch the left hand side it's not a pick
	//if (lhs_ys.empty()) {
	//	clearPick();
	//	return false;
	//}

	// If the pick touch either both of right hand side, bottom, and top, it's not a pick
	/*if ((dowmost.y > 0.9*bw.rows && rmost.x > 0.9*bw.cols) || (dowmost.y > 0.9*bw.rows && upmost.y < 0.1*bw.rows) || (rmost.x > 0.9*bw.cols && upmost.y < 0.1*bw.rows)) {
		cout << "Clear pick!" << endl;
		clearPick();
		return false;
	}*/



	// Enforce that the object is U-shaped, contacts the lhstwo separate times.
	// DISABLED FOR NOW - ODD PICK SHAPES / BENDS CAN LOOK NON-SEPARATED

	//std::sort(lhs_ys.begin(), lhs_ys.end());		/// Sort them
	//int dy = 0;							/// Get the max gap width, if any
	//for (int i = 0; i < lhs_ys.size() - 1; i++) {
	//	dy = max(lhs_ys[i + 1] - lhs_ys[i], dy);
	//}

	//if (dy < h / 25) {
	//	pick_coords.clear();
	//	return false;
	//}

	// Label the tip as the rightmost point
	//pick_tip_last_conv = pick_tip_conv;
	pick_tip_conv		= Point(rmost.x + Pick_ROI.x, rmost.y + Pick_ROI.y);;
	//pick_tip_rel_conv = Point2d((double)pick_tip.x / (double)w, (double)pick_tip.y / (double)h);

	//cout << "Conv  Pick x = " << pick_tip_conv.x << ", y = " << pick_tip_conv.y << endl;
	
	pick_tip_visible = true;


	// get information about the pick tip
	//computePickTipInfo(imgfilt, worms, h, w);

	return true;
}

double WormPick::otsu_8u_with_mask(const Mat src, const Mat &mask) {
	const int N = 256;
	int M = 0;
	int i, j, h[N] = { 0 };
	for (i = 0; i < src.rows; i++)
	{
		const uchar* psrc = src.ptr(i);
		const uchar* pmask = mask.ptr(i);
		for (j = 0; j < src.cols; j++)
		{
			if (pmask[j])
			{
				h[psrc[j]]++;
				++M;
			}
		}
	}

	double mu = 0, scale = 1. / (M);
	for (i = 0; i < N; i++)
		mu += i * (double)h[i];

	mu *= scale;
	double mu1 = 0, q1 = 0;
	double max_sigma = 0, max_val = 0;

	for (i = 0; i < N; i++)
	{
		double p_i, q2, mu2, sigma;

		p_i = h[i] * scale;
		mu1 *= q1;
		q1 += p_i;
		q2 = 1. - q1;

		if (std::min(q1, q2) < FLT_EPSILON || std::max(q1, q2) > 1. - FLT_EPSILON)
			continue;

		mu1 = (mu1 + i * p_i) / q1;
		mu2 = (mu - q1 * mu1) / q2;
		sigma = q1 * q2*(mu1 - mu2)*(mu1 - mu2);
		if (sigma > max_sigma)
		{
			max_sigma = sigma;
			max_val = i;
		}
	}

	return max_val;
}

double WormPick::threshold_with_mask(const Mat& src, Mat& dst, double thresh, double maxval, int type, const Mat& mask) {

	if (mask.empty() || (mask.rows == src.rows && mask.cols == src.cols && countNonZero(mask) == src.rows * src.cols))
	{
		// If empty mask, or all-white mask, use cv::threshold
		//thresh = cv::threshold(src, dst, thresh, maxval, type);
		//cout << "empty mask, or all - white mask" << endl;
		src.copyTo(dst);
	}
	else
	{
		// Use mask
		bool use_otsu = (type & THRESH_OTSU) != 0;
		if (use_otsu)
		{
			// If OTSU, get thresh value on mask only
			thresh = otsu_8u_with_mask(src, mask);
			// Remove THRESH_OTSU from type
			type &= THRESH_MASK;
		}

		// Apply cv::threshold on all image
		thresh = cv::threshold(src, dst, thresh, maxval, type);

		// Copy original image on inverted mask
		src.copyTo(dst, ~mask);
	}
	return thresh;
}


bool WormPick::DNNsegmentLoop(const Mat &imggray, const WormFinder &worms, Mat& imgfilt, cv::dnn::Net net) {
	
	// Basic variables
	Size	sz = imggray.size();
	int		w = sz.width;
	int		h = sz.height;
	//int		col_interval = 3;
	//double	minVal = 0;
	//Point	minLoc = Point(0, 0);
	//int min_pick_points = 0.25*w / col_interval;

	// Variables for DNN
	Mat img_inter, imgout; imggray.copyTo(img_inter);
	double dsfactor = 0.2;
	int display_class_idx = 0;
	double conf_thresh = 4;
	int norm_type = 1;
	// Don't both segmenting dark images?

	//Apply substantial smoothing to the image to filter out worms and other noise
	//GaussianBlur(imggray, imgfilt, Size(3, 3), 31, 31);
	imggray.copyTo(imgfilt);

	// Apply CNN to generate a mask of suspected region containing the pick
	findWormsInImage(img_inter, net, dsfactor, imgout, false, 2, display_class_idx, conf_thresh, norm_type);
	isolateLargestBlob(imgout, 30);
	Rect bound_box = boundingRect(imgout);

	Point center_of_bound_box = (bound_box.br() + bound_box.tl())*0.5;
	//Mat img_crop_rect(imgout.rows, imgout.cols, uchar(0));
	//Mat img_crop_rect = Mat::zeros(imgout.rows, imgout.cols, CV_8U);
	//rectangle(img_crop_rect, Rect((rect.x+0.7*rect.width), rect.y, 0.3*rect.width, rect.height), Scalar(255), CV_FILLED);
	//imshow("rectangle", img_crop_rect);
	//waitKey(1);
	//itwise_and(img_crop_rect, imgout, imgout);
	//img_crop_rect.copyTo(imgout);

	//bitwise_and(imgout, imggray, img_seg_dnn);
	//imshow("Binary_mask", imgout);
	//waitKey(1);
	// Within the masked region, thresholding method was used to find the pick contour
	//threshold_with_mask(imggray, img_thres, 100, 255, THRESH_OTSU, imgout);
	//imshow("threshold_with_balue", img_thres);
	//waitKey(1);
	//if (img_thres.empty() == false);
	//{
	//	img_thres.copyTo(imggray);
	//}
	// Find largest very dark object
	//Mat bw;
	////threshold(imgfilt, bw, 50, 255, THRESH_BINARY_INV); // orginal:50
	//adaptiveThreshold(imgfilt, bw, 255, ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY_INV, 91, 5);
	//isolateLargestBlob(bw, 80);// original:30



	// Get contours of largest blob
	//vector<vector<Point>> contours;
	//findContours(bw, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

	//// Downsample the contours to increase performance
	//pick_coords.clear();
	//if ((contours.size() >= 1)) {
	//	for (int i = 0; i < contours[0].size(); i += 3)
	//		pick_coords.push_back(contours[0][i]);
	//}
	//else {
	//	clearPick();
	//	return false;
	//}



	//// Extract all points that contact the left hand side. Also get the right-most point
	////vector<int> lhs_ys;
	//Point rmost(0, 0);
	//Point dowmost(0, 0);
	//Point upmost(0, 0);
	//for (int i = 0; i < pick_coords.size(); i++) {

	//	// Does this point touch the LHS?
	///*	if (pick_coords[i].x <= 30)
	//		lhs_ys.push_back(pick_coords[i].y);*/

	//		// Is this point towards the right (tip?)
	//	if (pick_coords[i].x > rmost.x)
	//		rmost = pick_coords[i];

	//	if (pick_coords[i].y > dowmost.y)
	//		dowmost = pick_coords[i];

	//	if (pick_coords[i].y < upmost.y)
	//		upmost = pick_coords[i];

	//}

	//// If the pick does not touch the left hand side it's not a pick
	////if (lhs_ys.empty()) {
	////	clearPick();
	////	return false;
	////}

	//// If the pick touch either both of right hand side, bottom, and top, it's not a pick
	//if ((dowmost.y > 0.9*bw.rows && rmost.x > 0.9*bw.cols) || (dowmost.y > 0.9*bw.rows && upmost.y < 0.1*bw.rows) || (rmost.x > 0.9*bw.cols && upmost.y < 0.1*bw.rows)) {
	//	clearPick();
	//	return false;
	//}



	//// Enforce that the object is U-shaped, contacts the lhstwo separate times.
	//// DISABLED FOR NOW - ODD PICK SHAPES / BENDS CAN LOOK NON-SEPARATED

	////std::sort(lhs_ys.begin(), lhs_ys.end());		/// Sort them
	////int dy = 0;							/// Get the max gap width, if any
	////for (int i = 0; i < lhs_ys.size() - 1; i++) {
	////	dy = max(lhs_ys[i + 1] - lhs_ys[i], dy);
	////}

	////if (dy < h / 25) {
	////	pick_coords.clear();
	////	return false;
	////}

	// Label the tip as the rightmost point
	pick_tip_last = pick_tip;
	pick_tip = center_of_bound_box;
	pick_tip_rel = Point2d((double)pick_tip.x / (double)w, (double)pick_tip.y / (double)h);

	pick_tip_visible = true;


	// get information about the pick tip
	computePickTipInfo(imgfilt, worms, h, w);

	return true;
}


void WormPick::computePickTipInfo(const Mat &imgfilt, const WormFinder &worms, int h, int w) {

	// Measure Y gradient as tenengrad variance (TENV), near tip if possible, otherwise in whole image
	int buffer = 0.05*h;
	int buffer_x = 0.05*w;
	int tenv_radius = 0.12*w;

	//if (pick_tip_visible) {
	//	pick_gradient = tenengrad(imgfilt, pick_tip, tenv_radius);
	//	pick_local_intensity = localIntensity(imgfilt, pick_tip, tenv_radius);
	//}
	//else {
	//	pick_gradient = tenengrad(imgfilt, pick_tip_last, tenv_radius);
	//	pick_local_intensity = localIntensity(imgfilt, pick_tip_last, tenv_radius);
	//}

	//// Add the pick radius to a history for the last 3 frames for less noisy radius measurement
	///// Add the current value to the end
	//pick_gradient_history.push_back(pick_gradient);

	///// Remove oldest value from beginning
	//pick_gradient_history.pop_front();

	///// Average to get the true pick gradient
	//double sum_of_deque = 0;
	//double num_in_deque = 0;
	//for (int n = 0; n < pick_gradient_history.size(); n++) {
	//	sum_of_deque += pick_gradient_history[n];
	//	num_in_deque++;
	//}
	//pick_gradient = sum_of_deque / num_in_deque;

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

}

// Thread function for finding Pick tip using CNN
void WormPick:: segmentPickCnn(const cv::Mat& imggray, const cv::dnn::Net& net, const WormFinder& worms, bool run_in_thread) {


	Size	sz = imggray.size();
	int		w = sz.width;
	int		h = sz.height;
	
	Pick_ROI = Rect(imggray.cols * 0.377, imggray.rows * 0.377, 640, 480);
	// Pick_ROI = Rect(imggray.cols / 2 - 320, imggray.rows /2 - 240, 640, 480);

	img_w = w;
	img_h = h;


	// TODO - Remove hardcodes 
	double dsfactor = 0.5; // 0.18
	double conf_thresh = 4.0;
	int norm_type = 1;


	do {
		// Make local copy of image for resizing
		Mat img, imgfilt;
		imggray(Pick_ROI).copyTo(img);

		//GaussianBlur(imggray(Pick_ROI), img, cv::Size(15,15),0 ,0);
		//imggray(ROI).copyTo(imgfilt);

		// Dummy imgout, we don't need it
		Mat imgout;

		// Run CNN to get output
		CnnOutput out = findWormsInImage(img, net, dsfactor, imgout, false, 0, 0, conf_thresh, norm_type);

		// Get the bin having highest confidence
		Point ROI_center = out.max_prob_point / dsfactor;

		// Get pick tip, represented by the center of mass of confidence in ROI
		vector<int> pts_x, pts_y;
		vector<double> pts_conf;
		for (int i = 0; i < out.coords.size(); i++) {
			if (ptDist(ROI_center, (out.coords[i] / dsfactor)) < 60) {
				pts_x.push_back(out.coords[i].x / dsfactor);
				pts_y.push_back(out.coords[i].y / dsfactor);
				pts_conf.push_back(out.probs[i]);
			}
		}

		double ctr_mass_x, ctr_mass_y;
		CenterofMass(ctr_mass_x, ctr_mass_y, pts_x, pts_y, pts_conf);
		

		
		if (ctr_mass_x == -1 || ctr_mass_y == -1) {
			pick_tip = out.max_prob_point / dsfactor;
		}
		else {
			pick_tip = Point(ctr_mass_x + Pick_ROI.x, ctr_mass_y + Pick_ROI.y);
			//cout << "Pick x = " << pick_tip.x << ", y = " << pick_tip.y << endl;
		}

		// Label the tip as the rightmost point
		//pick_tip_last = pick_tip;
		//pick_tip_rel = Point2d((double)pick_tip.x / (double)w, (double)pick_tip.y / (double)h);
		pick_tip_visible = true;
		// get information about the pick tip
		// computePickTipInfo(imgfilt, worms, h, w);

		// Check for exit
		boost_interrupt_point();

		// Don't run too fast
		// boost_sleep_ms(1000);

	} while (run_in_thread); // infinite loop if running in its own thread
}


void WormPick::clearPick() {
	// Clear existing pick
	pick_coords.clear();
	pick_tip = Point(-1, -1);
	pick_tip_rel = Point2d(0, 0);
	pick_tip_last = Point(-1, -1);
	pick_tip_visible = false;
}

bool WormPick::pickSweep(OxCnc& Grbl2, int sweep_dir) {
	
	// Set pick to the start position
	movePickToStartPos(Grbl2);

	// Enforce pick centering at image center
	while (!pick_tip_centered) {
		fineTuneCentering();
		boost_interrupt_point();
		boost_sleep_ms(25);
	}
	if (!in_pick_action) { return false; }

	// Update just the dynamixel centering positions for next time (do not update grbl position which may be too low)
	pick_center_2 = getServoPos(2);
	pick_center_3 = getServoPos(3);

	// Lower pick to worm
	lowerPickToTouch(Grbl2, Grbl2.getLabJackHandle());
	if (!in_pick_action) { return false; }

	// Surface contact is very shallow. Lower a tiny bit more.
	Grbl2.goToRelative(0, 0, 0.25, true);
	boost_sleep_ms(100);

	// Sweep pick to "scoop" worm
	for (int i = 0; i < 13; i++) {
		Dxl.moveServo(2, sweep_dir, true, true, false);
		boost_sleep_ms(60);
		if (!in_pick_action) { return false; }
	}
	boost_sleep_ms(500);

	// Lift worm away
	movePickSlightlyAway();
	if (!in_pick_action) { return false; }

	// Check whether worm was picked
	return true;

}

// Fine tune pick centering (1 step only in each axis)
bool WormPick::fineTuneCentering(bool do_x, bool do_y) {

	// Step in Y direction towards center using DXL motor 2
	if (!(pick_tip_centered_y == 0) && pick_tip_visible && do_y) {
		if (pick_tip_centered_y == 1) { Dxl.moveServo(2, -1, DXL_RELATIVE, false, true); }
		else { Dxl.moveServo(2, 1, true, false, true); }
		boost_sleep_ms(50);
	}

	// Step in X direction towards center using DXL motor 1
	if (!(pick_tip_centered_x == 0) && pick_tip_visible && do_x) {
		if (pick_tip_centered_x == 1) { Dxl.moveServo(1, 1, DXL_RELATIVE, false, true); }
		else { Dxl.moveServo(1, -1, true, false, true); }
		boost_sleep_ms(10);
	}

	return true;
}

// Fine tune pick centering to a CUSTOM centerpoint
bool WormPick::fineTuneCentering(double x_target_rel, double y_target_rel, bool do_x, bool do_y) {

	// Get location relative to CUSTOM centerpoint
	double buffer = 0.04;
	int ctr_x = 0;
	if (pick_tip_rel.x > x_target_rel + buffer && do_x)		{ ctr_x = 1; }
	else if (pick_tip_rel.x < x_target_rel - buffer && do_x) { ctr_x = -1; }

	int ctr_y = 0;
	if (pick_tip_rel.y > y_target_rel + buffer && do_y) { ctr_y = 1; }
	else if (pick_tip_rel.y < y_target_rel - buffer && do_y) { ctr_y = -1; }

	// Step in Y direction towards center using DXL motor 2
	if (!(ctr_y ==0) && pick_tip_visible && do_y) {
		if (ctr_y == 1) { Dxl.moveServo(2, -1, DXL_RELATIVE, false, true); }
		else { Dxl.moveServo(2, 1, true, false, true); }
		boost_sleep_ms(50);
	}

	// Step in X direction towards center using DXL motor 1
	if (!(ctr_x == 0) && pick_tip_visible && do_x) {
		if (ctr_x == 1) { Dxl.moveServo(1, 1, DXL_RELATIVE, false, true); }
		else { Dxl.moveServo(1, -1, true, false, true); }
		boost_sleep_ms(50);
	}

	
	bool is_done = ctr_x == 0 && ctr_y == 0; /// Note, if not doing ctr_x, for example, it never changes from 0.

	return is_done;
}

// Move pick to the target position then focus it
void WormPick::localizeAndFocus(OxCnc &Grbl2, long lngHandle) {
	// Move the pick to the target position
	// TODO
	/*
	1. get the coordinate of pick tip (Point(ctr_mass_x + Pick_ROI.x, ctr_mass_y + Pick_ROI.y);)
	2. hardcode the coordinate of target position (Size	sz = imggray.size();)
	3. move the pick tip to the target position
	*/
	// DX1.moveServo(1, man_int, DXL_RELATIVE,true);
	int curr_x = pick_tip.x;
	int curr_y = pick_tip.y;

	cout << curr_x << endl;
	cout << curr_y << endl;

	int targ_x = 1453;
	int targ_y = 970;

	int dir_x = 1;
	int dir_y = 1;
	
	int step = 10;
	// Dxl.moveServo(2, pick_center_2, false, false, false);
	// Dxl.moveServo(1, pick_start_1, false, true, false);

	while (abs(targ_x - curr_x) > 10 && curr_x < 1587 && curr_x > 1096) {
		dir_x = -abs(targ_x - curr_x) / (targ_x - curr_x);
		cout << "Moved servo 1 by " << dir_x << endl;
		// Dxl.moveServo(1, Dxl.positionOfServo(1) + dir_x * step, false, true, false);
		Dxl.moveServo(1, 2 * dir_x, true, true);
		boost_sleep_ms(100);
	}

	while (abs(targ_y - curr_y) > 10 && curr_y < 1182 && curr_y > 759) {
		dir_y = -abs(targ_y - curr_y) / (targ_y - curr_y);
		cout << "Moved servo 2 by " << dir_y << endl;
		Dxl.moveServo(2, 2 * dir_y, true, true);
		boost_sleep_ms(100);
	}
	

	/*
	Size	sz = imggray.size();
	int		target_x = sz.width;
	int		target_y = sz.height;
	*/
	
	// Focus it using laser
	// TODO
	/*
	1. get the laser shining image
	2. choose the ROI and move pick up and down to find the ROI with highest intensity
		(or outout the image with the smallest variance of brightness)
		based on observation, the area is more bright and brightness is more concentrated when the pick is focused.
	*/
}

// Lower pick to focal plane OR sudden intensity drop
void WormPick::lowerPickToFocus(OxCnc &Grbl2, long lngHandle) {
	
	// If lowering to Focus is called, use a larger interval
	lower_increment = 1;
	continue_lowering = true;

	// Clear lowering metrics
	focus_tenv.clear();
	focus_mean.clear();
	focus_AIN.clear();
	fail_count = 0;

	while (continue_lowering) {

		// Check whether lowering was signalled, if not do nothing and clear the focus curve
		boost_interrupt_point();

		// If signalled, but before lowering, enforce pick centering. Pause to re-center if needed.
		/*while (continue_lowering && !pick_tip_centered) {
			fineTuneCentering();
			boost_sleep_ms(10);
			boost_interrupt_point();
		}*/

		// If signalled, lower pick by 1 degree until in focus
		if (continue_lowering && pick_tip_visible) {

			/// If we're just starting the lowering, set some things up 
			if (focus_tenv.size() == 0) {

				/// Seed the focus curve with the current tenengrad / intensity values
				focus_tenv.push_back(pick_gradient);
				focus_mean.push_back(pick_local_intensity);
				focus_AIN.push_back(AIN0);

			}

			/// Adjust the lowering increment if we are terminate to the surface's last known height
			double quick_move_offset = 2; /// Positive number, subtracted later. 
			if (Grbl2.getZ() > focus_height - quick_move_offset)
				lower_increment = 0.25;
			else
				lower_increment = 1.0;

			/// Move Down unless we've reached the limit of movement
			if (!Grbl2.goToRelative(0, 0, lower_increment*lower_direction, true)) {
				cout << "Reached limit while lowering pick...\n";
				continue_lowering = false;
				return;
			}
			boost_sleep_ms(75);


			///  If any of the "stop-lowering" metrics suggests surface contact, then wait a moment 
			///		and triple check to confirm it wasn't a movement / vibration artefact.
			///		*if TENV decreased, waiting allows us to confirm it wasn't noise
			///		*if local intensity decreases suddenly due to digging into the agar, it should actually 
			///			get smaller and smaller on each subsequent check
			///		*if LabJack DAC0 detected surface contact it should continue to detect during double checks
			///		*if the current height is terminate to the last surface contact height

			int tries = 0;
			double focus_mean_diff_stop = -3, focus_mean_diff_slow = -1; /// Using "slow" value because only need to slow down and wait for capacitor
			double pick_intensity_diff = 0;

			while ((pick_gradient <= focus_tenv[focus_tenv.size() - 1] && tries < 3) ||
				(pick_local_intensity - focus_mean[focus_tenv.size() - 1] < focus_mean_diff_slow && tries < 3) ||
				(AIN0 > 2 && tries < 3) ||
				(Grbl2.getZ() > focus_height - quick_move_offset && tries < 3))
			{
				boost_sleep_ms(150 * tries);
				printf("Checking for stop: %0.2f\t%0.2f\t%0.2f\t%0.2f/%0.2f\n", pick_gradient, pick_local_intensity, AIN0, Grbl2.getZ(), focus_height);
				tries += 1;
			}

			focus_tenv.push_back(pick_gradient);
			focus_mean.push_back(pick_local_intensity);
			focus_AIN.push_back(AIN0);
			int idx = (int) focus_tenv.size() - 1;
			pick_gradient_diff = focus_tenv[idx] - focus_tenv[idx - 1];
			pick_intensity_diff = focus_mean[idx] - focus_mean[0];

			///// DEBUG: Display focus curve
			printf("----------------------------------------\n");
			for (int i = 0; i < focus_tenv.size(); i++)
				printf("%0.2f\t%0.2f\t%0.2f\n", focus_tenv[i], focus_mean[i], focus_AIN[i]);
			printf("\n");

			/// Determine whether to continue lowering or not
			continue_lowering = pick_gradient_diff > pick_gradient_diff_thresh &&
				pick_intensity_diff > focus_mean_diff_stop &&
				AIN0 < 2;

			/// If we have finished lowering, then save the lowering position
			if (!continue_lowering)
				focus_height = Grbl2.getZ();
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


// Raise the pick until it is either in focus or getting more out of focus
void WormPick::raisePickToFocus(OxCnc& Grbl2) {

	lower_direction = -1; /// Reverse lowering direction, more negative is up.
	lowerPickToFocus(Grbl2, Grbl2.getLabJackHandle());
	lower_direction = 1;

}

void WormPick::heatPick(OxCnc &Grbl2, int DacNum, int secs_to_heat, double secs_to_cool) {
	AnthonysTimer ttimer;
	cap_sensor_ready = false;
	heating_pick = true;
	// Close the switch-relays first to enable heating (disables touch sensation)
	LabJack_SetDacN(Grbl2.getLabJackHandle(), 1, 0); // Turn off the capacitive sensor
	LabJack_SetFioN(Grbl2.getLabJackHandle(), 6, 0);

	string err_string = string("");

	AnthonysTimer sterilize_timer;

	// Send request to labjack to heat pic
	LabJack_Pulse_DAC(Grbl2.getLabJackHandle(), DacNum, secs_to_heat, 0, err_string, 5, 0);

	// Open the switch-relays to disable heating (re-enables touch sensation)
	LabJack_SetFioN(Grbl2.getLabJackHandle(), 6, 5);

	// Reset the capacitive sensor, takes some time to recover after opening both heating relays.
	powerCycleCapSensor(Grbl2.getLabJackHandle());

	while (sterilize_timer.getElapsedTime() < secs_to_heat + secs_to_cool) {
		boost_sleep_ms(20);
	}

	//cap_sensor_ready = false;
	heating_pick = false;
	cout << "Done heating pick: " << ttimer.getElapsedTime() << endl;
}

void WormPick::heatPickInThread(OxCnc &Grbl2, WormPick &Pk, int DacNum, int secs_to_heat, double secs_to_cool) {
	boost::thread sterilize_thread = boost::thread(&WormPick::heatPick, &Pk, boost::ref(Grbl2), DacNum, secs_to_heat, secs_to_cool);
}

void WormPick::powerCycleCapSensor(long lngHandle) {
	/*int max_wait = 120;
	AnthonysTimer wait_timer;
	while (LabJack_ReadAINN(lngHandle, 0) > 2 && wait_timer.getElapsedTime() < max_wait) {
		boost_sleep_ms(1000);
		cout << "Waiting for cap sensor reading to return to baseline." << endl;
	}*/

	Err.notify(errors::cap_power_cycle);
	start_reading_for_touch = false;
	LabJack_SetFioN(lngHandle, 6, 5); // turn on the relay 
	LabJack_SetDacN(lngHandle, 1, 0.0); // Turn off the capacitive sensor
	//LabJack_SetFioN(lngHandle, 7, 1);
	boost_sleep_ms(200);
	LabJack_SetDacN(lngHandle, 1, 5.0); // Turn the sensor back on. It will now auto calibrate to whatever medium it is in (should be air).
	//LabJack_SetFioN(lngHandle, 7, 0);
	boost_sleep_ms(200);
	cout << "^^^^^^^^^^^^^^^^^^^^ AIN0 from power cycle: " << LabJack_ReadAINN(lngHandle, 0) << endl;
	start_reading_for_touch = true;
	cap_sensor_ready = true;
}

// Lower pick until it touches the agar surface
// TODO: Remove Grbl 2
void WormPick::lowerPickToTouch(OxCnc &Grbl2, long lngHandle, bool power_cycle, string plate_id) {
	start_reading_for_touch = false;
	continue_lowering = true; /// Signal that we are lowering

	bool lower_pick_more = true;
	// If lowering is triggered, first power-cycle the capacative touch sensor
	/// can be disabled if lptt is running as an addendum to another picking method, like quickPick();
	int tries = 0;
	/*do {
		cout << "Power cycling the capacitive sensor." << endl;
		powerCycleCapSensor(lngHandle);
		tries++;
		boost_sleep_ms(50);
		cout << "^^^^^^^^^^^^^^^^^^^^ AIN0: " << LabJack_ReadAINN(lngHandle, 0) << endl;
	} while (!lower_pick_more && tries < 3);*/
	powerCycleCapSensor(lngHandle);
	cout << "Done power cycling the capacitive sensor." << endl;
	//if (power_cycle) {
	//	//continue_lowering = true;	/// Signal that we are lowering
	//	powerCycleCapSensor(lngHandle);

	//}

	double max_lower_distance = 500;
	double distance_lowered = 0;


	cout << "Using capacitive sensor to lower pick to agar." << endl;

	// Lower slowly. Assumes other movements have brought the pick terminate
	while (lower_pick_more && pick_tip_visible && distance_lowered < max_lower_distance) {

		lower_increment = 20;
		distance_lowered += lower_increment;

		if (!Actx.moveRelative(lower_increment, true)) {
			cout << "Reached limit while lowering pick...\n";
			lower_pick_more = false;
			return;
		}

		AIN0 = LabJack_ReadAINN(lngHandle, 0);
		cout << "^^^^^^^^^^^^^^^^^^^^ AIN0 lowering: " << AIN0 << endl;
		if (AIN0 > 2) {
			lower_pick_more = false;
			cout << "Cap sensor triggered: " << AIN0 << endl;
		}

		cout << "			Lowering pick, distance Lowered: " << distance_lowered << endl;
		boost_sleep_ms(10);
	}
	

	// End AIN0 reads
	start_reading_for_touch = false;

	// If we have finished lowering, then save the lowering position
	/*if (!continue_lowering)
		focus_height = Actx.readPosition();*/

	//boost_sleep_ms(50);
	boost_interrupt_point();

}

/*
	000000000		0 - OOB
	000010000		1 - not searched
	001111100		2 - searched, still blobs
	001121100		3 - search, no blobs
	000010000
	000000000
*/

void WormPick::pickWander(OxCnc& Ox, OxCnc &Grbl2, WormFinder& worms) {
	
	return;
}

//bool WormPick::quickPick(OxCnc& Ox, OxCnc &Grbl2, WormFinder& worms,
//																		int dig, 
//																		int sweep, 
//																		double pause, 
//																		bool restore_height_after,
//																		bool integrate_worm_centering,
//																		pick_mode selectionMode)
//{
//	Err.notify(PRINT_ONLY, "\n\n---------- Starting quickPick() ----------\n");
//
//	// Reset the user's judgement and wait a moment to ensure a gap in the file 
//	action_success_user = -1;
//	boost_sleep_ms(500);
//
//	// Store pickup parameters in case changes aren't meant to learn/stick on this operation
//	double focus_height_bak = focus_height;
//	action_success = false; /// set successful pick/drop to false. Will be checked later.
//
//	// Allow several tries in case of failures
//	int tries = 0;
//	bool success_positioned = false; // Whether or not the pick is successfully positioned ready for the final lowering
//	int start_offset = -20;			 // Start with a very generous offset in case of pick bends
//
//	// Set X lowering position
//	double x_lower_rel = 0.63;
//	double y_lower_rel = 0.40;
//
//	// Keep track of whether the pick is currently directly above the worm or offset
//	bool worm_offset_status = 1; /// Centering moves worm to the offset position
//	
//	while (++tries < 4  && !success_positioned) {
//
//		// Alow user to abort without trying again
//		if (abort_lowering) {
//			abort_lowering = false;
//			cout << "ABORT 1" << endl;
//			return false;
//		}
//
//		/*
//			STEP I: Position the pick just above the agar and to the right of the worm.
//		*/
//
//		Err.notify(PRINT_ONLY, "STEP I - Try " + to_string(tries) + " - Position pick");
//
//		// Verify that the pick is at the focus height
//		Grbl2.goToAbsolute(Grbl2.getX(), Grbl2.getY(), getPickupHeight(), true, 20); 
//
//		// Power cycle the capacitive sensor so we can check for touch
//		powerCycleCapSensor(Grbl2.getLabJackHandle());
//		continue_lowering = true;
//
//
//		// Move pick to "start" location, everything in place to touch EXCEPT servo 1 slightly up
//		//		e.g. move servos only, with an offset on serv1, without grbl (grbl would send 
//		//		it up in Z to the true start position)
//		if (!movePickToStartPos(Grbl2, 0, 0, start_offset))
//			continue;
//
//		/*
//			STEP I-a: [Optional] Position the worm 1 dydOx unit below center point
//		*/
//		AnthonysTimer Ta;
//		if (integrate_worm_centering) {
//
//			/// If we aren't offset, then go to the offset position and re-find the worm
//			if (!worm_offset_status) {
//				worm_offset_status = Ox.goToRelative(0, -1, 0, 1, true);
//				boost_sleep_ms(150);
//				if (!worms.findWormNearestPickupLoc()) {
//					return false;
//				}
//				
//			}
//
//			/// Verify that the worm is centered at the offset position (5s time limit)
//			helperCenterWorm(Ta, 5, worms, Ox);
//		}
//
//
//		// Center pick in Y
//		AnthonysTimer Ty;
//		boost_sleep_ms(200);
//		while (	!fineTuneCentering(x_lower_rel, y_lower_rel, false, true) && Ty.getElapsedTime() < 5){}
//			
//		pick_center_2 = getServoPos(2); /// New Y center position, esp if going to have trouble later.
//
//		Err.notify(PRINT_ONLY, "\ty-center (d2) changed to: " + to_string(pick_center_2));
//
//		// Exception 1: The pick touched AND:
//			// it is left of the center, AND
//			// We're still on the first try, OR
//			// Pick is far left (regardless of whether it's touching).
//			// Increase the servo 1 position
//		if ((!continue_lowering && pick_tip_visible && pick_tip_rel.x < x_lower_rel-0.02 && tries < 2) || pick_tip_rel.x < x_lower_rel - 0.05) {
//			Err.notify(PRINT_ONLY, "\tE1 in STEP I - try " + to_string(tries));
//
//			// Raise servo 1
//			pick_start_1 -= 5;
//			movePickToStartPos(Grbl2, 0, start_offset); /// Move immediately before can reset continue_lowering
//			Err.notify(PRINT_ONLY, "\tpick_start_1 changed to: " + to_string(pick_start_1));
//
//			boost_sleep_ms(250);
//			continue_lowering = true;
//			continue;
//
//		}
//
//		// Exception 2: The pick touched and is actually at the right position (by accident...)
//			// Continue to final lowering, which should be automatically skipped by continue_lowering = false.
//		else if (!continue_lowering && pick_tip_visible &&
//			pick_tip_rel.x < x_lower_rel &&
//			pick_tip_rel.x > x_lower_rel-0.1) 
//		{
//			Err.notify(PRINT_ONLY, "\tE2 in STEP I - continuing");
//			/// Do nothing, just proceed to step II
//		}
//
//		// Exception 3: Pick touched but we either can't see or or it's on the right or we already tried once. Raise Z axis. 
//		else if (!continue_lowering) {
//			Err.notify(PRINT_ONLY, "\tE3 in STEP I - try " + to_string(tries));
//
//			//Raise Z
//			focus_height = getPickupHeight() - 3;
//			Grbl2.goToAbsolute(Grbl2.getX(), Grbl2.getY(), focus_height, true, 20);
//			boost_sleep_ms(500);
//			Err.notify(PRINT_ONLY, "\tfocus_height changed to: " + to_string(focus_height));
//
//			// Continue lowering needs to be re-set to true. The autoscanner can only set it to false, not reset to true
//			continue_lowering = true;
//			continue;
//		}
//
//		// Exception 4: The tip is not visible. Need to re-center
//		else if (!pick_tip_visible) {
//
//			Err.notify(PRINT_ONLY,"E4 in STEP I - need to center");
//			continue_lowering = false;
//			return false;
//		}
//
//		// Otherwise, completed step 1!
//		else {
//			Err.notify(PRINT_ONLY, "\tSTEP I complete. Ready for final drop");
//		}
//
//		// Allow user to abort without trying again
//		if (abort_lowering) {
//			abort_lowering = false;
//			cout << "ABORT 2" << endl;
//
//			return false;
//		}
//
//		/*
//			STEP Ia (optional): confirm worm is still centered at the offset point and eliminate the offset
//		*/
//
//		if (integrate_worm_centering && worm_offset_status) {
//			worm_offset_status = !Ox.goToRelative(0, 1, 0, 1.0, true, 5);
//			boost_sleep_ms(150);
//			fineTuneCentering(false, true); /// Step pick Y centering in case of vibrations moving it around
//		}
//
//		// Alow user to abort without trying again
//		if (abort_lowering) {
//			abort_lowering = false;
//			cout << "ABORT 3" << endl;
//
//			return false;
//		}
//
//		/*
//			STEP II: Final lowering of the pick
//		*/
//
//		double step2buff = 0.05;
//		Err.notify(PRINT_ONLY, "STEP II - Try " + to_string(tries) + " - Final lower");
//
//		// If pick looks good, perform final dip into the agar SLOWLY (start pos with NO offset on servo1)
//		// Stop if:
//		//		- We've complete the entire final dip, OR
//		//		- The tip is no longer visible, OR
//		//		- The tip has touched the surface (continue lowering is 0), OR
//		//		- The tip has moved too far left (remaining lowering must be done vertically from the center position). 
//
//		if (pick_tip_rel.y > y_lower_rel - 0.1 && pick_tip_rel.y < y_lower_rel + 0.1 && pick_tip_visible) {
//			Err.notify(PRINT_ONLY, "\tCentered and visible, perform final lowering...");
//
//			for (int i = 0; 
//				 i < abs(start_offset*2) && pick_tip_visible && continue_lowering && pick_tip_rel.x > x_lower_rel; 
//				 i++) 
//			{
//				// Lower by 1 step
//				moveServo(1, 1, true, true);
//				boost_sleep_ms(100);
//
//				// If Y has gone OOB, adjust it
//				fineTuneCentering(x_lower_rel, y_lower_rel,false, true);
//			}
//		}
//
//		// Exception 5: Can't see the pick anymore
//		else {
//			Err.notify(PRINT_ONLY, "\tE5 in STEP II - retrying");
//			continue;
//		}
//
//		// Exception 6: Pick hit the ground before reaching center. If so move up and try again.
//		boost_sleep_ms(150);
//		if (!continue_lowering && pick_tip_rel.x > x_lower_rel) {
//			Err.notify(PRINT_ONLY,"\tE6 in STEP II - try " + to_string(tries));
//			boost_sleep_ms(50);
//			focus_height = getPickupHeight() - 1.5;
//			continue;
//		}
//
//		// Exception 7: Pick lowered all the way but did not hit ground OR center. Decrease starting angle
//		if (continue_lowering && pick_tip_rel.x > x_lower_rel + step2buff) {
//			Err.notify(PRINT_ONLY, "\tE7 in STEP II - try " + to_string(tries));
//			boost_sleep_ms(50);
//			pick_start_1 += 5;
//			continue;
//		}
//
//		// One last attempt to center Y, assuming it isn't touching yet. 
//		if (continue_lowering)
//			fineTuneCentering(false, true);
//
//		// Did we succeed? centered?
//		success_positioned =	pick_tip_rel.x > x_lower_rel- step2buff &&
//								pick_tip_rel.x < x_lower_rel+ step2buff &&
//								pick_tip_rel.y > y_lower_rel- step2buff &&
//								pick_tip_rel.y < y_lower_rel+ step2buff;
//
//		Err.notify(PRINT_ONLY, "\tSTEP 2 succeeded (1/0)? - " + to_string(success_positioned));
//
//
//	}
//
//	// If we tried 3 times and still couldn't position properly, exit failure
//	if (!success_positioned) {
//		Err.notify(PRINT_ONLY, "\tGAVE UP.");
//		return false;
//	}
//
//	// Alow user to abort without trying again
//	if (abort_lowering) {
//		abort_lowering = false;
//		cout << "ABORT 4" << endl;
//
//		return false;
//	}
//
//	/*
//		STEP IIa (optional): 
//			If we're using offset lowering mode and the pick isn't touching yet, 
//			the worm had a chance to crawl away.
//			Re-center worm before vertical lowering (don't raise the pick up).
//	*/
//
//
//	if (integrate_worm_centering && continue_lowering) {
//		Err.notify(PRINT_ONLY, "\tRunning StepIIa offset");
//		/// If we aren't offset, then go to the offset position and re-find the worm
//		if (!worm_offset_status) {
//			worm_offset_status = Ox.goToRelative(0, -1, 0, 1, true);
//			boost_sleep_ms(150);
//			if (!worms.findWormNearestPickupLoc())
//				return false;
//		}
//
//		/// Verify that the worm is centered at the offset position (5s time limit)
//		AnthonysTimer Ta;
//		helperCenterWorm(Ta, 5, worms, Ox);
//
//		/// Move the pick back over the worm to allow lowering correctly
//		worm_offset_status = Ox.goToRelative(0, 1, 0, 1, true);
//
//	}
//
//	// If aborted, don't do any followup steps
//	if (abort_lowering) {
//		abort_lowering = false;
//		cout << "ABORT 5" << endl;
//
//		return false;
//	}
//
//
//	
//	/*
//			STEP III: Finally, X and Y are centered, but STILL not touching? lower vertically. Save center position
//						Then, sweep and raise up.
//	*/
//	Err.notify(PRINT_ONLY, "STEP III: Vertical lower if not touching.");
//
//	if (sweep < 0)
//	{
//		Dxl.moveServo(3, 8, true, true);
//		Dxl.moveServo(2, 8, true, true);
//		cout << "Tilting pick" << endl;
//	}
//
//	lowerPickToTouch(Grbl2, Grbl2.getLabJackHandle(), false);
//	Err.notify(PRINT_ONLY, "\tfocus_height changed to: " + to_string(focus_height));
//
//
//	// Alow user to abort without trying again
//	if (abort_lowering) {
//		abort_lowering = false;
//		cout << "ABORT 6" << endl;
//
//		return false;
//	}
//	else if (!continue_lowering) {
//		setPickCenter(Dxl.positionOfServo(1), Dxl.positionOfServo(2), Dxl.positionOfServo(3), Grbl2.getY(), Grbl2.getZ() - 5, Grbl2.getZ());
//	}
//
//	// Optionally add an extra vertical dig to push deeper into the agar
//	moveServo(1, dig, true, true, false);
//	Err.notify(PRINT_ONLY, "\tDig servo 1 by " + to_string(dig));
//
//	// Wait a moment to let worm stick
//	boost_sleep_ms(250);
//
//	// Alow user to abort without trying again
//	if (abort_lowering) {
//		abort_lowering = false;
//		cout << "ABORT 7" << endl;
//
//		return false;
//	}
//	
//	if (sweep < 0) 
//	{
//		boost_sleep_ms(pause * 1000);// Optionally wait after the sweep
//	}
//
//	// Optionally sweep the pick in y
//	Err.notify(PRINT_ONLY, "\tSweeping increments: " + to_string(sweep));
//	for (int i = 0; i < abs(sweep); i++) {
//		moveServo(2, 1*sweep / abs(sweep), true, true);
//		boost_sleep_ms(600);
//	}
//
//	// Optionally wait after the sweep 
//	boost_sleep_ms(pause * 1000);
//
//	// Raise pick
//	Err.notify(PRINT_ONLY, "\tMoving pick away...");
//
//	if (sweep < 0)
//	{
//		for (int i = 0; i < 15; i++) {
//			moveServo(1, -1, true, true);
//			boost_sleep_ms(700);
//		}
//	}
//
//	movePickSlightlyAway();
//
//	// Make sure continue_lowering is off (should be)
//	continue_lowering = false;
//	Err.notify(PRINT_ONLY, "\tCompleted STEP III.");
//
//	// If height learning / adapting is to be blocked, restore the old focus height values
//	if (restore_height_after) {
//		focus_height = focus_height_bak;
//		Err.notify(PRINT_ONLY, "\tRestored original focus_height.");
//	}
//
//
//	// If in DROP mode, wait a bit to check that the worm is off
//	if (selectionMode == DROP) {
//		if (worms.wasWormDropped()) {
//			Err.notify(PRINT_ONLY, "successfully dropped worm\n");
//		}
//		else {
//			Err.notify(PRINT_ONLY, "failed to drop worm\n");
//			// TODO try again?
//		}
//	}
//
//	//If in AUTO pick mode, verify the worm was picked
//	if (selectionMode == AUTO) {
//		if (!worms.mvmtAtPickedSite()) {
//			worms.setPickedWorm();
//			action_success = true;
//			Err.notify(PRINT_ONLY, "successfully picked worm\n");
//		}
//		else {
//			Err.notify(PRINT_ONLY, "failed to pick worm\n");
//			action_success = false;
//			return false;
//		}
//	}
//
//	// Clear abort flag if reached here and it's still on
//	if (abort_lowering) {
//		abort_lowering = false;
//		cout << "ABORT 8" << endl;
//
//		return false;
//	}
//	else {
//		
//
//		return true;
//	}
//}

//bool WormPick::AutoquickPick(OxCnc& Ox, OxCnc &Grbl2, WormFinder& worms,
//																		int dig,
//																		int sweep,
//																		double pause,
//																		bool restore_height_after,
//																		bool integrate_worm_centering,
//																		pick_mode selectionMode)
//{
//	Err.notify(PRINT_ONLY, "\n\n---------- Starting quickPick() ----------\n");
//
//	// Reset the user's judgement and wait a moment to ensure a gap in the file 
//	action_success_user = -1;
//	boost_sleep_ms(500);
//
//	// Store pickup parameters in case changes aren't meant to learn/stick on this operation
//	double focus_height_bak = focus_height;
//	action_success = false; /// set successful pick/drop to false. Will be checked later.
//
//	// Allow several tries in case of failures
//	int tries = 0;
//	bool success_positioned = false; // Whether or not the pick is successfully positioned ready for the final lowering
//	int start_offset = -20;			 // Start with a very generous offset in case of pick bends
//
//	// Set X lowering position
//	double x_lower_rel = 0.63;
//	double y_lower_rel = 0.40;
//
//	// Keep track of whether the pick is currently directly above the worm or offset
//	bool worm_offset_status = 1; /// Centering moves worm to the offset position
//
//	while (++tries < 4 && !success_positioned) {
//
//		// Alow user to abort without trying again
//		if (abort_lowering) {
//			abort_lowering = false;
//			cout << "ABORT 1" << endl;
//			return false;
//		}
//
//		/*
//			STEP I: Position the pick just above the agar and to the right of the worm.
//		*/
//
//		Err.notify(PRINT_ONLY, "STEP I - Try " + to_string(tries) + " - Position pick");
//
//		// Verify that the pick is at the focus height
//		Grbl2.goToAbsolute(Grbl2.getX(), Grbl2.getY(), getPickupHeight(), true, 20);
//
//		// Power cycle the capacitive sensor so we can check for touch
//		powerCycleCapSensor(Grbl2.getLabJackHandle());
//		continue_lowering = true;
//
//		// Move pick to "start" location, everything in place to touch EXCEPT servo 1 slightly up
//		//		e.g. move servos only, with an offset on serv1, without grbl (grbl would send 
//		//		it up in Z to the true start position)
//		if (!movePickToStartPos(Grbl2, 0, 0, start_offset))
//			continue;
//
//
//
//		/*
//		STEP I-a: [Optional] Position the worm 1 dydOx unit below center point
//		*/
//		AnthonysTimer Ta;
//		if (integrate_worm_centering) {
//
//			/// If we aren't offset, then go to the offset position and re-find the worm
//			if (worm_offset_status) {
//				worm_offset_status = Ox.goToRelative(0, -1, 0, 1, true);
//				cout << worm_offset_status << endl;
//				boost_sleep_ms(150);
//				if (!worms.findWormNearestPickupLocCNN()) {
//					cout << "findWormNearestPickupLocCNN1 fail" << endl;
//					return false;
//				}
//
//			}
//
//			/// Verify that the worm is centered at the offset position (5s time limit)
//			helperCenterWormCNN(Ta, 5, worms, Ox);
//		}
//
//
//		
//		// Center pick in Y
//		AnthonysTimer Ty;
//		boost_sleep_ms(200);
//		while (!fineTuneCentering(x_lower_rel, y_lower_rel, false, true) && Ty.getElapsedTime() < 5) {}
//
//		pick_center_2 = getServoPos(2); /// New Y center position, esp if going to have trouble later.
//
//		Err.notify(PRINT_ONLY, "\ty-center (d2) changed to: " + to_string(pick_center_2));
//
//		// Exception 1: The pick touched AND:
//			// it is left of the center, AND
//			// We're still on the first try, OR
//			// Pick is far left (regardless of whether it's touching).
//			// Increase the servo 1 position
//		if ((!continue_lowering && pick_tip_visible && pick_tip_rel.x < x_lower_rel - 0.02 && tries < 2) || pick_tip_rel.x < x_lower_rel - 0.05) {
//			Err.notify(PRINT_ONLY, "\tE1 in STEP I - try " + to_string(tries));
//
//			// Raise servo 1
//			pick_start_1 -= 5;
//			movePickToStartPos(Grbl2, 0, start_offset); /// Move immediately before can reset continue_lowering
//			Err.notify(PRINT_ONLY, "\tpick_start_1 changed to: " + to_string(pick_start_1));
//
//			boost_sleep_ms(250);
//			continue_lowering = true;
//			continue;
//
//		}
//
//		// Exception 2: The pick touched and is actually at the right position (by accident...)
//			// Continue to final lowering, which should be automatically skipped by continue_lowering = false.
//		else if (!continue_lowering && pick_tip_visible &&
//			pick_tip_rel.x < x_lower_rel &&
//			pick_tip_rel.x > x_lower_rel - 0.1)
//		{
//			Err.notify(PRINT_ONLY, "\tE2 in STEP I - continuing");
//			/// Do nothing, just proceed to step II
//		}
//
//		// Exception 3: Pick touched but we either can't see or or it's on the right or we already tried once. Raise Z axis. 
//		else if (!continue_lowering) {
//			Err.notify(PRINT_ONLY, "\tE3 in STEP I - try " + to_string(tries));
//
//			//Raise Z
//			focus_height = getPickupHeight() - 3;
//			Grbl2.goToAbsolute(Grbl2.getX(), Grbl2.getY(), focus_height, true, 20);
//			boost_sleep_ms(500);
//			Err.notify(PRINT_ONLY, "\tfocus_height changed to: " + to_string(focus_height));
//
//			// Continue lowering needs to be re-set to true. The autoscanner can only set it to false, not reset to true
//			continue_lowering = true;
//			continue;
//		}
//
//		// Exception 4: The tip is not visible. Need to re-center
//		else if (!pick_tip_visible) {
//
//			Err.notify(PRINT_ONLY, "E4 in STEP I - need to center");
//			continue_lowering = false;
//			return false;
//		}
//
//		// Otherwise, completed step 1!
//		else {
//			Err.notify(PRINT_ONLY, "\tSTEP I complete. Ready for final drop");
//		}
//
//		// Allow user to abort without trying again
//		if (abort_lowering) {
//			abort_lowering = false;
//			cout << "ABORT 2" << endl;
//
//			return false;
//		}
//
//		/*
//			STEP Ia (optional): confirm worm is still centered at the offset point and eliminate the offset
//		*/
//
//		if (integrate_worm_centering && worm_offset_status) {
//			//worm_offset_status = !Ox.goToRelative(0, 1, 0, 1.0, true, 5);
//			boost_sleep_ms(150);
//			fineTuneCentering(false, true); /// Step pick Y centering in case of vibrations moving it around
//		}
//
//		// Alow user to abort without trying again
//		if (abort_lowering) {
//			abort_lowering = false;
//			cout << "ABORT 3" << endl;
//
//			return false;
//		}
//
//		/*
//			STEP II: Final lowering of the pick
//		*/
//
//		double step2buff = 0.05;
//		Err.notify(PRINT_ONLY, "STEP II - Try " + to_string(tries) + " - Final lower");
//
//		// If pick looks good, perform final dip into the agar SLOWLY (start pos with NO offset on servo1)
//		// Stop if:
//		//		- We've complete the entire final dip, OR
//		//		- The tip is no longer visible, OR
//		//		- The tip has touched the surface (continue lowering is 0), OR
//		//		- The tip has moved too far left (remaining lowering must be done vertically from the center position). 
//
//		if (pick_tip_rel.y > y_lower_rel - 0.1 && pick_tip_rel.y < y_lower_rel + 0.1 && pick_tip_visible) {
//			Err.notify(PRINT_ONLY, "\tCentered and visible, perform final lowering...");
//
//			for (int i = 0;
//				i < abs(start_offset * 2) && pick_tip_visible && continue_lowering && pick_tip_rel.x > x_lower_rel;
//				i++)
//			{
//				// Lower by 1 step
//				moveServo(1, 1, true, true);
//				boost_sleep_ms(100);
//
//				// If Y has gone OOB, adjust it
//				fineTuneCentering(x_lower_rel, y_lower_rel, false, true);
//			}
//		}
//
//		// Exception 5: Can't see the pick anymore
//		else {
//			Err.notify(PRINT_ONLY, "\tE5 in STEP II - retrying");
//			continue;
//		}
//
//		// Exception 6: Pick hit the ground before reaching center. If so move up and try again.
//		boost_sleep_ms(150);
//		if (!continue_lowering && pick_tip_rel.x > x_lower_rel) {
//			Err.notify(PRINT_ONLY, "\tE6 in STEP II - try " + to_string(tries));
//			boost_sleep_ms(50);
//			focus_height = getPickupHeight() - 1.5;
//			continue;
//		}
//
//		// Exception 7: Pick lowered all the way but did not hit ground OR center. Decrease starting angle
//		if (continue_lowering && pick_tip_rel.x > x_lower_rel + step2buff) {
//			Err.notify(PRINT_ONLY, "\tE7 in STEP II - try " + to_string(tries));
//			boost_sleep_ms(50);
//			pick_start_1 += 5;
//			continue;
//		}
//
//		// One last attempt to center Y, assuming it isn't touching yet. 
//		if (continue_lowering)
//			fineTuneCentering(false, true);
//
//		// Did we succeed? centered?
//		success_positioned = pick_tip_rel.x > x_lower_rel - step2buff &&
//			pick_tip_rel.x < x_lower_rel + step2buff &&
//			pick_tip_rel.y > y_lower_rel - step2buff &&
//			pick_tip_rel.y < y_lower_rel + step2buff;
//
//		Err.notify(PRINT_ONLY, "\tSTEP 2 succeeded (1/0)? - " + to_string(success_positioned));
//
//
//	}
//
//	// If we tried 3 times and still couldn't position properly, exit failure
//	if (!success_positioned) {
//		Err.notify(PRINT_ONLY, "\tGAVE UP.");
//		return false;
//	}
//
//	// Alow user to abort without trying again
//	if (abort_lowering) {
//		abort_lowering = false;
//		cout << "ABORT 4" << endl;
//
//		return false;
//	}
//
//	/*
//		STEP IIa (optional):
//			If we're using offset lowering mode and the pick isn't touching yet,
//			the worm had a chance to crawl away.
//			Re-center worm before vertical lowering (don't raise the pick up).
//	*/
//
//
//	if (integrate_worm_centering && continue_lowering) {
//		Err.notify(PRINT_ONLY, "\tRunning StepIIa offset");
//		/// If we aren't offset, then go to the offset position and re-find the worm
//		cout << "worm_offset_status = "<< worm_offset_status << endl;
//		if (worm_offset_status) {
//			worm_offset_status = Ox.goToRelative(0, -1, 0, 1, true);
//			boost_sleep_ms(150);
//			if (!worms.findWormNearestPickupLocCNN()) {
//				cout << "findWormNearestPickupLocCNN2 fail" << endl;
//				return false;
//			}
//		}
//
//		/// Verify that the worm is centered at the offset position (5s time limit)
//		AnthonysTimer Ta;
//		helperCenterWormCNN(Ta, 5, worms, Ox);
//
//		/// Move the pick back over the worm to allow lowering correctly
//		//worm_offset_status = Ox.goToRelative(0, 1, 0, 1, true);
//
//	}
//
//	// If aborted, don't do any followup steps
//	if (abort_lowering) {
//		abort_lowering = false;
//		cout << "ABORT 5" << endl;
//
//		return false;
//	}
//
//
//
//	/*
//			STEP III: Finally, X and Y are centered, but STILL not touching? lower vertically. Save center position
//						Then, sweep and raise up.
//	*/
//	Err.notify(PRINT_ONLY, "STEP III: Vertical lower if not touching.");
//	lowerPickToTouch(Grbl2, Grbl2.getLabJackHandle(), false);
//	Err.notify(PRINT_ONLY, "\tfocus_height changed to: " + to_string(focus_height));
//
//
//	// Allow user to abort without trying again
//	if (abort_lowering) {
//		abort_lowering = false;
//		cout << "ABORT 6" << endl;
//
//		return false;
//	}
//	else if (!continue_lowering) {
//		setPickCenter(Dxl.positionOfServo(1), Dxl.positionOfServo(2), Dxl.positionOfServo(3), Grbl2.getY(), Grbl2.getZ() - 5, Grbl2.getZ());
//	}
//
//	// Optionally add an extra vertical dig to push deeper into the agar
//	moveServo(1, dig, true, true, false);
//	Err.notify(PRINT_ONLY, "\tDig servo 1 by " + to_string(dig));
//
//	// Wait a moment to let worm stick
//	boost_sleep_ms(250);
//
//	// Alow user to abort without trying again
//	if (abort_lowering) {
//		abort_lowering = false;
//		cout << "ABORT 7" << endl;
//
//		return false;
//	}
//
//	// Optionally sweep the pick in y
//	Err.notify(PRINT_ONLY, "\tSweeping increments: " + to_string(sweep));
//	for (int i = 0; i < abs(sweep); i++) {
//		moveServo(2, 1 * sweep / abs(sweep), true, true);
//		boost_sleep_ms(75);
//	}
//
//	// Optionally wait after the sweep 
//	boost_sleep_ms(pause * 1000);
//
//	// Raise pick
//	Err.notify(PRINT_ONLY, "\tMoving pick away...");
//
//	if (sweep > 0)
//		VmovePickSlightlyAway(-1, 0, -1); // Input arguments: (stepsz1-3) step size of motor 1-3. (0 means disabled, not move)
//											//stepsz 1&3 < 0; stepsz 2 > 0;
//	if (sweep < 0)
//		VmovePickSlightlyAway(1, 0, 1);
//
//	// Make sure continue_lowering is off (should be)
//	continue_lowering = false;
//	Err.notify(PRINT_ONLY, "\tCompleted STEP III.");
//
//	// If height learning / adapting is to be blocked, restore the old focus height values
//	if (restore_height_after) {
//		focus_height = focus_height_bak;
//		Err.notify(PRINT_ONLY, "\tRestored original focus_height.");
//	}
//
//
//	// If in DROP mode, wait a bit to check that the worm is off
//	//if (selectionMode == DROP) {
//	//	if (worms.wasWormDropped()) {
//	//		Err.notify(PRINT_ONLY, "successfully dropped worm\n");
//	//	}
//	//	else {
//	//		Err.notify(PRINT_ONLY, "failed to drop worm\n");
//	//		// TODO try again?
//	//	}
//	//}
//
//	////If in AUTO pick mode, verify the worm was picked
//	//if (selectionMode == AUTO) {
//	//	if (!worms.mvmtAtPickedSite()) {
//	//		worms.setPickedWorm();
//	//		action_success = true;
//	//		Err.notify(PRINT_ONLY, "successfully picked worm\n");
//	//	}
//	//	else {
//	//		Err.notify(PRINT_ONLY, "failed to pick worm\n");
//	//		action_success = false;
//	//		return false;
//	//	}
//	//}
//
//	// Clear abort flag if reached here and it's still on
//	if (abort_lowering) {
//		abort_lowering = false;
//		cout << "ABORT 8" << endl;
//
//		return false;
//	}
//	else {
//
//
//		return true;
//	}
//}




bool PrintOutMetrics(vector<Point> start_pts, vector<Point> end_pts, vector<int> pix_diff, vector<double> confs, Point pickup_pt, double alpha, double beta, double thresh) {

	int len = start_pts.size();
	vector <bool> status;
	bool success = true;
	if (end_pts.size() == len && pix_diff.size() == len && confs.size() == len && len > 0) {
		for (int i = 0; i < len; i++) {
			double this_mvm = ptDist(start_pts[i], end_pts[i]);
			double this_start_rad = ptDist(pickup_pt, start_pts[i]);
			double this_end_rad = ptDist(pickup_pt, end_pts[i]);
			double this_energy = alpha * (pix_diff[i] - 8000) / 8000 + beta * (confs[i] - 6) / 6;
			cout << "Worm " << i << " :" << endl;
			cout << "Start radius = " << this_start_rad << endl;
			cout << "End radius = " << this_end_rad << endl;
			cout << "Travel distance = " << this_mvm << endl;
			cout << "Pixel change = " << pix_diff[i] << endl;
			cout << "Confidence level = " << confs[i] << endl;
			cout << "Energy value = " << this_energy << endl;
			
			if (this_energy > thresh) {
				status.push_back(true);
			}
			else {
				status.push_back(false);
			}
		}


		for (int j = 0; j < status.size(); j++) {
			if (status[j] == true) { 
				success = false;  break; 
			}
		}

		cout << "Worm picked up? " << success << endl;
		return success;
	
	}

	else {
		cout << "Cannot perform analysis: No worm-like object detected!" << endl;
		return false;
	}

}





//bool WormPick::CNNtrackquickPick(OxCnc& Ox, OxCnc &Grbl2, WormFinder& worms,
//																		int dig,
//																		int sweep,
//																		double pause,
//																		bool restore_height_after,
//																		bool integrate_worm_centering,
//																		pick_mode selectionMode)
//{
//	Err.notify(PRINT_ONLY, "\n\n---------- Starting quickPick() ----------\n");
//
//	// Reset the user's judgement and wait a moment to ensure a gap in the file 
//	action_success_user = -1;
//	boost_sleep_ms(500);
//
//	// Store pickup parameters in case changes aren't meant to learn/stick on this operation
//	double focus_height_bak = focus_height;
//	action_success = false; /// set successful pick/drop to false. Will be checked later.
//
//	// Allow several tries in case of failures
//	int tries = 0;
//	bool success_positioned = false; // Whether or not the pick is successfully positioned ready for the final lowering
//	int start_offset = -20;			 // Start with a very generous offset in case of pick bends
//
//	// Set X lowering position
//	double x_lower_rel = 0.63;
//	double y_lower_rel = 0.40;
//
//	// Keep track of whether the pick is currently directly above the worm or offset
//	bool worm_offset_status = 1; /// Centering moves worm to the offset position
//
//	while (++tries < 4 && !success_positioned) {
//
//		// Alow user to abort without trying again
//		if (abort_lowering) {
//			abort_lowering = false;
//			cout << "ABORT 1" << endl;
//			return false;
//		}
//
//		/*
//			STEP I: Position the pick just above the agar and to the right of the worm.
//		*/
//
//		Err.notify(PRINT_ONLY, "STEP I - Try " + to_string(tries) + " - Position pick");
//
//		// Verify that the pick is at the focus height
//		Grbl2.goToAbsolute(Grbl2.getX(), Grbl2.getY(), getPickupHeight(), true, 20);
//
//		// Power cycle the capacitive sensor so we can check for touch
//		powerCycleCapSensor(Grbl2.getLabJackHandle());
//		continue_lowering = true;
//
//		//////////////////////////////////////////////////////////////// original code
//		if (!movePickToStartPos(Grbl2, 0, 0, start_offset))
//			// continue; // uncomment
//
//
//		/*
//		STEP I-a: [Optional] Position the worm 1 dydOx unit below center point
//		*/
//
//		// Wait for a few second to find a worm to track
//		boost_sleep_ms(5000);
//		worms.worm_finding_mode = 1;
//		worms.trackwormNearPcikupLoc = false;
//
//		
//		AnthonysTimer Ta;
//		if (integrate_worm_centering) {
//
//			Dxl.moveServo(2, -15, true, true);
//
//			/// If we aren't offset, then go to the offset position and re-find the worm
//			if (worm_offset_status) {
//				//worm_offset_status = Ox.goToRelative(0, -0.5, 0, 1, true); // TO DO: offset
//				//cout << worm_offset_status << endl;
//				boost_sleep_ms(150);
//				if (!worms.getWormNearestPickupLocCNN()) {
//					cout << "getWormNearestPickupLocCNN1 fail" << endl;
//					return false;
//				}
//
//			}
//
//			/// Verify that the worm is centered at the offset position (5s time limit)
//			helperCenterWormCNNtrack(Ta, 12, worms, Ox);
//
//			Dxl.moveServo(2, 15, true, true);
//		}
//
//		//return true; // to comment
//		
//
//		// Move pick to "start" location, everything in place to touch EXCEPT servo 1 slightly up
//		//		e.g. move servos only, with an offset on serv1, without grbl (grbl would send 
//		//		it up in Z to the true start position)
//		
//		
//		
//		// Center pick in Y
//		AnthonysTimer Ty;
//		boost_sleep_ms(200);
//		while (!fineTuneCentering(x_lower_rel, y_lower_rel, false, true) && Ty.getElapsedTime() < 5) {}
//
//		pick_center_2 = getServoPos(2); /// New Y center position, esp if going to have trouble later.
//
//		Err.notify(PRINT_ONLY, "\ty-center (d2) changed to: " + to_string(pick_center_2));
//
//		// Exception 1: The pick touched AND:
//			// it is left of the center, AND
//			// We're still on the first try, OR
//			// Pick is far left (regardless of whether it's touching).
//			// Increase the servo 1 position
//		if ((!continue_lowering && pick_tip_visible && pick_tip_rel.x < x_lower_rel - 0.02 && tries < 2) || pick_tip_rel.x < x_lower_rel - 0.05) {
//			Err.notify(PRINT_ONLY, "\tE1 in STEP I - try " + to_string(tries));
//
//			// Raise servo 1
//			// pick_start_1 -= 5;
//			movePickToStartPos(Grbl2, 0, start_offset); /// Move immediately before can reset continue_lowering
//			Err.notify(PRINT_ONLY, "\tpick_start_1 changed to: " + to_string(pick_start_1));
//
//			boost_sleep_ms(250);
//			continue_lowering = true;
//			// continue; // uncomment
//
//		}
//
//		// Exception 2: The pick touched and is actually at the right position (by accident...)
//			// Continue to final lowering, which should be automatically skipped by continue_lowering = false.
//		else if (!continue_lowering && pick_tip_visible &&
//			pick_tip_rel.x < x_lower_rel &&
//			pick_tip_rel.x > x_lower_rel - 0.1)
//		{
//			Err.notify(PRINT_ONLY, "\tE2 in STEP I - continuing");
//			/// Do nothing, just proceed to step II
//		}
//
//		// Exception 3: Pick touched but we either can't see or or it's on the right or we already tried once. Raise Z axis. 
//		else if (!continue_lowering) {
//			Err.notify(PRINT_ONLY, "\tE3 in STEP I - try " + to_string(tries));
//
//			//Raise Z
//			// focus_height = getPickupHeight() - 3;
//			Grbl2.goToAbsolute(Grbl2.getX(), Grbl2.getY(), focus_height, true, 20);
//			boost_sleep_ms(500);
//			Err.notify(PRINT_ONLY, "\tfocus_height changed to: " + to_string(focus_height));
//
//			// Continue lowering needs to be re-set to true. The autoscanner can only set it to false, not reset to true
//			continue_lowering = true;
//			// continue;
//		}
//
//		// Exception 4: The tip is not visible. Need to re-center
//		else if (!pick_tip_visible) {
//
//			Err.notify(PRINT_ONLY, "E4 in STEP I - need to center");
//			continue_lowering = false;
//			return false;
//		}
//
//		// Otherwise, completed step 1!
//		else {
//			Err.notify(PRINT_ONLY, "\tSTEP I complete. Ready for final drop");
//		}
//		
//
//		// Allow user to abort without trying again
//		
//		if (abort_lowering) {
//			abort_lowering = false;
//			cout << "ABORT 2" << endl;
//
//			return false;
//		}
//
//		/*
//			STEP Ia (optional): confirm worm is still centered at the offset point and eliminate the offset
//		*/
//		
//		
//		if (integrate_worm_centering && worm_offset_status) {
//			//worm_offset_status = !Ox.goToRelative(0, 1, 0, 1.0, true, 5);
//			boost_sleep_ms(150);
//			fineTuneCentering(false, true); /// Step pick Y centering in case of vibrations moving it around
//		}
//
//		// Alow user to abort without trying again
//		if (abort_lowering) {
//			abort_lowering = false;
//			cout << "ABORT 3" << endl;
//
//			return false;
//		}
//		
//
//		/*
//			STEP II: Final lowering of the pick
//		*/
//
//
//		
//		double step2buff = 0.05;
//		Err.notify(PRINT_ONLY, "STEP II - Try " + to_string(tries) + " - Final lower");
//
//		// If pick looks good, perform final dip into the agar SLOWLY (start pos with NO offset on servo1)
//		// Stop if:
//		//		- We've complete the entire final dip, OR
//		//		- The tip is no longer visible, OR
//		//		- The tip has touched the surface (continue lowering is 0), OR
//		//		- The tip has moved too far left (remaining lowering must be done vertically from the center position). 
//
//		if (pick_tip_rel.y > y_lower_rel - 0.1 && pick_tip_rel.y < y_lower_rel + 0.1 && pick_tip_visible) {
//			Err.notify(PRINT_ONLY, "\tCentered and visible, perform final lowering...");
//
//			for (int i = 0;
//				i < abs(start_offset * 2) && pick_tip_visible && continue_lowering && pick_tip_rel.x > x_lower_rel;
//				i++)
//			{
//				// Lower by 1 step
//				moveServo(1, 1, true, true);
//				boost_sleep_ms(100);
//
//				// If Y has gone OOB, adjust it
//				fineTuneCentering(x_lower_rel, y_lower_rel, false, true);
//			}
//		}
//
//		// Exception 5: Can't see the pick anymore
//		else {
//			Err.notify(PRINT_ONLY, "\tE5 in STEP II - retrying");
//			continue;
//		}
//
//		// Exception 6: Pick hit the ground before reaching center. If so move up and try again.
//		boost_sleep_ms(150);
//		if (!continue_lowering && pick_tip_rel.x > x_lower_rel) {
//			Err.notify(PRINT_ONLY, "\tE6 in STEP II - try " + to_string(tries));
//			boost_sleep_ms(50);
//			focus_height = getPickupHeight() - 1.5;
//			continue;
//		}
//
//		// Exception 7: Pick lowered all the way but did not hit ground OR center. Decrease starting angle
//		if (continue_lowering && pick_tip_rel.x > x_lower_rel + step2buff) {
//			Err.notify(PRINT_ONLY, "\tE7 in STEP II - try " + to_string(tries));
//			boost_sleep_ms(50);
//			// pick_start_1 += 5;
//			// continue;
//		}
//
//		// One last attempt to center Y, assuming it isn't touching yet. 
//		if (continue_lowering)
//			fineTuneCentering(false, true);
//
//		// Did we succeed? centered?
//		success_positioned = pick_tip_rel.x > x_lower_rel - step2buff &&
//			pick_tip_rel.x < x_lower_rel + step2buff &&
//			pick_tip_rel.y > y_lower_rel - step2buff &&
//			pick_tip_rel.y < y_lower_rel + step2buff;
//
//		Err.notify(PRINT_ONLY, "\tSTEP 2 succeeded (1/0)? - " + to_string(success_positioned));
//		
//
//	}
//
//	// If we tried 3 times and still couldn't position properly, exit failure
//	
//	if (!success_positioned) {
//		Err.notify(PRINT_ONLY, "\tGAVE UP.");
//		return false;
//	}
//	
//
//	// Alow user to abort without trying again
//	if (abort_lowering) {
//		abort_lowering = false;
//		cout << "ABORT 4" << endl;
//
//		return false;
//	}
//
//	/*
//		STEP IIa (optional):
//			If we're using offset lowering mode and the pick isn't touching yet,
//			the worm had a chance to crawl away.
//			Re-center worm before vertical lowering (don't raise the pick up).
//	*/
//
//	
//	if (integrate_worm_centering && continue_lowering) {
//		Dxl.moveServo(2, -15, true, true);
//		Err.notify(PRINT_ONLY, "\tRunning StepIIa offset");
//		/// If we aren't offset, then go to the offset position and re-find the worm
//		cout << "worm_offset_status = " << worm_offset_status << endl;
//		if (worm_offset_status) {
//			//worm_offset_status = Ox.goToRelative(0, -1, 0, 1, true);
//			boost_sleep_ms(150);
//			if (!worms.getWormNearestPickupLocCNN()) {
//				cout << "getWormNearestPickupLocCNN2 fail" << endl;
//				return false;
//			}
//		}
//
//		/// Verify that the worm is centered at the offset position (5s time limit)
//		AnthonysTimer Ta;
//		helperCenterWormCNNtrack(Ta, 7, worms, Ox);
//
//		/// Move the pick back over the worm to allow lowering correctly
//		//worm_offset_status = Ox.goToRelative(0, 1, 0, 1, true);
//		Dxl.moveServo(2, 15, true, true);
//	}
//
//
//	// If aborted, don't do any followup steps
//	if (abort_lowering) {
//		abort_lowering = false;
//		cout << "ABORT 5" << endl;
//
//		return false;
//	}
//
//
//
//	/*
//			STEP III: Finally, X and Y are centered, but STILL not touching? lower vertically. Save center position
//						Then, sweep and raise up.
//	*/
//
//	Err.notify(PRINT_ONLY, "STEP III: Vertical lower if not touching.");
//	lowerPickToTouch(Grbl2, Grbl2.getLabJackHandle(), false);
//	Err.notify(PRINT_ONLY, "\tfocus_height changed to: " + to_string(focus_height));
//
//
//	// Allow user to abort without trying again
//	if (abort_lowering) {
//		abort_lowering = false;
//		cout << "ABORT 6" << endl;
//
//		return false;
//	}
//	else if (!continue_lowering) {
//		setPickCenter(Dxl.positionOfServo(1), Dxl.positionOfServo(2), Dxl.positionOfServo(3), Grbl2.getY(), Grbl2.getZ() - 5, Grbl2.getZ());
//	}
//
//	// Optionally add an extra vertical dig to push deeper into the agar
//	moveServo(1, dig, true, true, false);
//	Err.notify(PRINT_ONLY, "\tDig servo 1 by " + to_string(dig));
//
//	// Wait a moment to let worm stick
//	boost_sleep_ms(250);
//
//	// Alow user to abort without trying again
//	if (abort_lowering) {
//		abort_lowering = false;
//		cout << "ABORT 7" << endl;
//
//		return false;
//	}
//
//	// Optionally sweep the pick in y
//	Err.notify(PRINT_ONLY, "\tSweeping increments: " + to_string(sweep));
//	for (int i = 0; i < abs(sweep); i++) {
//		moveServo(2, 1 * sweep / abs(sweep), true, true);
//		boost_sleep_ms(10);
//	}
//
//	// Optionally wait after the sweep 
//	boost_sleep_ms(pause * 1000);
//
//	// Raise pick
//	Err.notify(PRINT_ONLY, "\tMoving pick away...");
//
//	VmovePickSlightlyAway(-1, 0, -1); // Input arguments: (stepsz1-3) step size of motor 1-3. (0 means disabled, not move)
//									  //stepsz 1&3 < 0; stepsz 2 > 0;
//
//	// Make sure continue_lowering is off (should be)
//	continue_lowering = false;
//	Err.notify(PRINT_ONLY, "\tCompleted STEP III.");
//
//
//
//	//////////////////////////////////////////////////////////////////////////////
//	// Start verfiy process
//
//	if (selectionMode == AUTO) {
//		worms.findWormNearPickupLoc = 1;
//		worms.worm_finding_mode = 0;
//
//		// Verify whether worm is picked up
//		/*double total_movement = worms.WormMvmFromPickedSite();
//		if (total_movement != 0) {
//			cout << "Total movement of worm to pick = " << total_movement << " (Pixel distance)" << endl;
//		}*/
//
//		Ox.goToRelative(0, 0.7, 0, 1, true);
//
//		boost_sleep_ms(1000);
//
//		worms.trackwormNearPcikupLoc = true;
//		worms.worm_finding_mode = 1;
//
//		//worms.trackbyCtrofMass = false;
//
//		//boost_sleep_ms(200);
//
//		//worms.limit_max_dist = false;
//		//worms.findWormNearPickupLoc = true;
//		//boost_sleep_ms(200);
//		//worms.findWormNearPickupLoc = false;
//
//		//worms.trackbyCtrofMass = false;
//		//boost_sleep_ms(2000);
//
//		/////////////////////////////////////////////////////////////////// TO DO: Put it into worms
//		//int monitor_time = 20;
//		//double total_len = 0;
//
//		// worms.max_move = 20;
//		//worms.worm_finding_mode = 1;
//
//		//Point last_position = worms.tracked_worm.getWormCentroid();
//		vector<Point> start_cents = worms.getcentroidAll();
//
//		boost_sleep_ms(5000);
//
//		////////Point current_position = worms.tracked_worm.getWormCentroid();
//		vector<Point> end_cents = worms.getcentroidAll();
//
//		vector<int> pixdiff = worms.getPixelDiff();
//		vector<double> conf = worms.getConfiSinglefrmCnn();
//		double alpha = 1, beta = 1, thresh = 0;
//
//		bool picked_up = PrintOutMetrics(start_cents, end_cents, pixdiff, conf, worms.getPickupLocation(), alpha, beta, thresh);
//
//		//cout << "Worm movement " << ptDist(last_position, current_position) << endl;
//
//
//		//worms.trackbyCtrofMass = true;
//		//worms.max_move = 10;
//
//		/*vector<int> pixdiff = worms.getPixelDiff();
//		for (int i = 0; i < pixdiff.size(); i++) {
//			cout << "Pixel change " << pixdiff[i] << endl;
//		}
//
//		vector<double> conf = worms.getConfiSinglefrmCnn();
//		for (int i = 0; i < conf.size(); i++) {
//			cout << "Confidence level " << conf[i] << endl;
//		}*/
//
//		//double alpha = 1, beta = 1;
//		//if ((!pixdiff.empty()) && (!conf.empty())) {
//		//	double energy_value = alpha * (pixdiff[0] - 8000) / 8000 + beta * (conf[0] - 6) / 6;
//		//	cout << "Energy Value = " << energy_value << endl;
//		//	if (energy_value > 0) {
//		//		cout << "Worm NOT picked up" << endl;
//		//	}
//		//	else {
//		//		cout << "Worm picked up" << endl;
//		//	}
//		//}
//
//		worms.worm_finding_mode = 0;
//		worms.findWormNearPickupLoc = false;
//		worms.trackwormNearPcikupLoc = false;
//
//		return picked_up;
//	
//	}
//	//worms.trackbyCtrofMass = true;
//
//	//worms.limit_max_dist = true;
//
//	//// Measure the total movement distance of picked worm right after picking
//	//for (int i = 0; i < monitor_time; i++) {
//
//	//	boost_sleep_ms(1000);
//	//	current_position = worms.tracked_worm.getWormCentroid();
//
//	//	if (last_position != Point(-1, -1) && last_position != Point(-1, -1)) {
//	//		total_len += ptDist(last_position, current_position);
//	//		last_position = current_position;
//	//	}
//	//	else {
//	//		cout << "Worm lose tracked!" << endl;
//	//		break;
//	//		//return total_len;
//	//	}
//	//}
//
//	//boost_sleep_ms(5000);
//	//cout << "Total movement of worm to pick = " << total_len << " (Pixel distance)" << endl;
//	//for (int i = 0; i < worms.getPixelDiff().size(); i++) {
//	//	cout << "Worm movement " << worms.getPixelDiff()[i] << endl;
//	//}
//
//	/////////////////////////////////////////////////////////////////// TO DO: Put it into worms
//
//	// If height learning / adapting is to be blocked, restore the old focus height values
//	if (restore_height_after) {
//		focus_height = focus_height_bak;
//		Err.notify(PRINT_ONLY, "\tRestored original focus_height.");
//	}
//
//
//	// If in DROP mode, wait a bit to check that the worm is off
//	//if (selectionMode == DROP) {
//	//	if (worms.wasWormDropped()) {
//	//		Err.notify(PRINT_ONLY, "successfully dropped worm\n");
//	//	}
//	//	else {
//	//		Err.notify(PRINT_ONLY, "failed to drop worm\n");
//	//		// TODO try again?
//	//	}
//	//}
//
//	////If in AUTO pick mode, verify the worm was picked
//	//if (selectionMode == AUTO) {
//	//	if (!worms.mvmtAtPickedSite()) {
//	//		worms.setPickedWorm();
//	//		action_success = true;
//	//		Err.notify(PRINT_ONLY, "successfully picked worm\n");
//	//	}
//	//	else {
//	//		Err.notify(PRINT_ONLY, "failed to pick worm\n");
//	//		action_success = false;
//	//		return false;
//	//	}
//	//}
//
//	// Clear abort flag if reached here and it's still on
//	if (abort_lowering) {
//		abort_lowering = false;
//		cout << "ABORT 8" << endl;
//
//		return false;
//	}
//	else {
//
//
//		return true;
//	}
//}


bool WormPick::pickOrDropOneWormREFERENCE(OxCnc& Ox, OxCnc &Grbl2, WormFinder& worms,
														int dig,
														int sweep,
														double pause,
														bool restore_height_after,
														bool integrate_worm_centering,
														pick_mode selectionMode) 
{

	AnthonysTimer time_steps;
	Err.notify(PRINT_ONLY, "\n\n---------- Starting quickPick() ----------\n");



	cout << "dig  = " << dig << endl;
	cout << "sweep = " << sweep << endl;
	cout << "pause  = " << pause << endl;
	cout << "restore_height_after = " << restore_height_after << endl;
	cout << "integrate_worm_centering = " << integrate_worm_centering << endl;
	cout << "selectionMode = " << selectionMode << endl;



	// Reset the user's judgement and wait a moment to ensure a gap in the file 
	action_success_user = -1;
	// boost_sleep_ms(500);

	// Store pickup parameters in case changes aren't meant to learn/stick on this operation
	double focus_height_bak = focus_height;
	action_success = false; /// set successful pick/drop to false. Will be checked later.

	// Allow several tries in case of failures
	int tries = 0;
	bool success_positioned = false; // Whether or not the pick is successfully positioned ready for the final lowering
	int start_offset = 0;			 // -20 before change. Start with a very generous offset in case of pick bends
	int relative_gantry_movement = 20;


	// Set X lowering position
	double x_lower_rel = 0.63;
	double y_lower_rel = 0.40;

	// Set the offset distance
	double x_offset = 0.06; // 0.075
	double y_offset = -0.08; //-0.066

	// Set the history file.
	string historyFile = "C:\\WormPicker\\PickCalibration\\PickHistory20201220.txt";

	// Get the original pickup location
	Point orig_pickup_pt = worms.getPickupLocation();

	// Keep track of whether the pick is currently directly above the worm or offset
	bool worm_offset_status = 1; /// Centering moves worm to the offset position

	// Power cycle the capacitive sensor before we move the pick into position. This way we know the sensor is calibrated
	// before touching the agar and we can get good readings when we lower the pick.
	powerCycleCapSensor(Grbl2.getLabJackHandle());

	////If dropping a worm, then generate a random offset within the plate bounds at which to drop
	//if (sweep < 0) {
	//	srand(time(NULL));

	//	// Generate x and y offset
	//	double x_offset = (rand() % 100) / double(10); // Generate an offset between 0 and 10 mm
	//	double y_offset = (rand() % 200 - 100) / double(10); // Generate an offset between -10 and +10mm

	//	cout << "Random offset for drop: " << x_offset << ", " << y_offset << endl;

	//	// Move the stage according to the offset
	//	Ox.goToRelative(x_offset, y_offset, 0, 1, true, 0);

	//}

	
	

	while (++tries < 4 && !success_positioned) {
		cout << "			Speedpick start motion: " << time_steps.getElapsedTime() << endl;
		time_steps.setToZero();

		// Alow user to abort without trying again
		if (abort_lowering) {
			abort_lowering = false;
			cout << "ABORT 1" << endl;
			return false;
		}

		/*
			STEP I: Position the pick just above the agar and to the right of the worm.
		*/

		Err.notify(PRINT_ONLY, "STEP I - Try " + to_string(tries) + " - Position pick");

		// Verify that the pick is at the focus height

		cout << "***** Going to right above pick focus height *****" << endl;
		Grbl2.goToAbsolute(Grbl2.getX(), Grbl2.getY(), getPickupHeight()- pick_focus_height_offset, true, 0);

		cout << "			Done go to right above focus height: " << time_steps.getElapsedTime() << endl;
		time_steps.setToZero();

		// Power cycle the capacitive sensor so we can check for touch
		// powerCycleCapSensor(Grbl2.getLabJackHandle());
		//continue_lowering = true;

		// Raise the gantry up a little bit prevent hitting the agar
		//Ox.goToRelative(0, 0, relative_gantry_movement, 1, true, 9);

		// Use non careful mode to move the pick to start position
		if (!movePickToStartPos(Grbl2, 0, 0, start_offset, 0))
			continue;

		cout << "			Done move to start position (non careful): " << time_steps.getElapsedTime() << endl;
		time_steps.setToZero();

		// Wait for a few second to find a worm to track
		//Ox.goToRelative(0, 0, -relative_gantry_movement, 1, true, 9);

		// boost_sleep_ms(2500);
		//worms.worm_finding_mode = 1;
		//worms.trackwormNearPcikupLoc = false;

		// AnthonysTimer Ta;
		// 10/16 TO DO: COMMENT
		//if (integrate_worm_centering) {

		//	Dxl.moveServo(2, -15, true, true);

		//	/// If we aren't offset, then go to the offset position and re-find the worm
		//	if (worm_offset_status) {
		//		//worm_offset_status = Ox.goToRelative(0, -0.5, 0, 1, true); // TO DO: offset
		//		//cout << worm_offset_status << endl;
		//		boost_sleep_ms(150);
		//		if (!worms.getWormNearestPickupLocCNN()) {
		//			cout << "getWormNearestPickupLocCNN1 fail" << endl;
		//			return false;
		//		}

		//	}

		//	/// Verify that the worm is centered at the offset position (5s time limit)
		//	helperCenterWormCNNtrack(Ta, 12, worms, Ox);

		//	Dxl.moveServo(2, 15, true, true);
		//}


		// Move pick to "start" location, everything in place to touch EXCEPT servo 1 slightly up
		//		e.g. move servos only, with an offset on serv1, without grbl (grbl would send 
		//		it up in Z to the true start position)



		// Center pick in Y
		// AnthonysTimer Ty;
		//boost_sleep_ms(200);

		// while (!fineTuneCentering(x_lower_rel, y_lower_rel, false, true) && Ty.getElapsedTime() < 5) {}
		// 10/9 TO DO remove???: center pick in Y direction by going to calibration map

		// pick_center_2 = getServoPos(2); /// New Y center position, esp if going to have trouble later.

		Err.notify(PRINT_ONLY, "\ty-center (d2) changed to: " + to_string(pick_center_2));

		// Completed step 1!
		Err.notify(PRINT_ONLY, "\tSTEP I complete. Ready for final drop");

		// Allow user to abort without trying again
		if (abort_lowering) {
			abort_lowering = false;
			cout << "ABORT 2" << endl;

			return false;
		}

		/*
		STEP II: Final lowering of the pick
		*/

		double step2buff = 0.05;
		Err.notify(PRINT_ONLY, "STEP II - Try " + to_string(tries) + " - Final lower");

		// Raise up the grbl2
		// Grbl2.goToAbsolute(Grbl2.getX(), Grbl2.getY(), Grbl2.getZ() - pick_focus_height_offset, true, 0);

		
		///////////////////////// COMMENT OUT THE CALIBRATION MAPPING JUST FOR BEBUG, PUT THEM BACK FOR REAL APPLICATION

		////// Set the pickup location to the current position of tracked worm
		////if (sweep > 0) {
		////	Point current_worm_position = worms.getTrackedCentroid();
		////	worms.setPickupLocation(current_worm_position.x, current_worm_position.y);
		////}

		////// Calculate the offset position according to the tracked worm's position
		////// TO DO: Check
		////target_pt = Point(worms.pickup_location.x + x_offset * (double)img_w, worms.pickup_location.y + y_offset * (double)img_h);





		////// Directly move the pick right above the offset position (the point where the pick hit the agar, near to the worm)
		////if (withinCalibrMap(target_pt)) {
		////	if (!MovePickbyCalibrMap(Grbl2, target_pt, historyFile)) { return false; }
		////	cout << "target point: x = " << target_pt.x * (double)img_w << ", y = " << target_pt.y * (double)img_h << endl;
		////}
		////else {
		////	cout << "The pick-up point is out of the calibration map!" << endl;
		////	worms.setPickupLocation(orig_pickup_pt.x, orig_pickup_pt.y);
		////	return false;
		////}


		/////////////////////////

		// Did we succeed? centered?
		/*success_positioned = pick_tip_rel.x > x_lower_rel - step2buff &&
			pick_tip_rel.x < x_lower_rel + step2buff &&
			pick_tip_rel.y > y_lower_rel - step2buff &&
			pick_tip_rel.y < y_lower_rel + step2buff;

		Err.notify(PRINT_ONLY, "\tSTEP 2 succeeded (1/0)? - " + to_string(success_positioned));*/

		success_positioned = true; // uncomment
	
	
	}

	

	// If we tried 3 times and still couldn't position properly, exit failure

	if (!success_positioned) {
		Err.notify(PRINT_ONLY, "\tGAVE UP.");
		return false;
	}


	// Alow user to abort without trying again
	if (abort_lowering) {
		abort_lowering = false;
		cout << "ABORT 4" << endl;

		return false;
	}

	/*
	STEP IIa (optional):
		If we're using offset lowering mode and the pick isn't touching yet,
		the worm had a chance to crawl away.
		Re-center worm before vertical lowering (don't raise the pick up).
	*/

	cout << "integrate_worm_centering = " << integrate_worm_centering << endl;
	cout << "continue_lowering = " << continue_lowering << endl;
	cout << "sweep = " << sweep << endl;

	//continue_lowering = true;

	if (integrate_worm_centering && sweep > 0) {
		////Dxl.moveServo(2, -15, true, true);
		//Err.notify(PRINT_ONLY, "\tRunning StepIIa offset");
		///// If we aren't offset, then go to the offset position and re-find the worm
		//cout << "worm_offset_status = " << worm_offset_status << endl;
		//if (worm_offset_status) {
		//	//worm_offset_status = Ox.goToRelative(0, -1, 0, 1, true);
		//	// boost_sleep_ms(150);
		//	if (!worms.getWormNearestPickupLocCNN()) {
		//		cout << "getWormNearestPickupLocCNN2 fail" << endl;
		//		return false;
		//	}
		//}

		///// Verify that the worm is centered at the offset position (5s time limit)
		//AnthonysTimer Ta;
		//helperCenterWormCNNtrack(Ta, 7, worms, Ox);

		///// Move the pick back over the worm to allow lowering correctly
		////worm_offset_status = Ox.goToRelative(0, 1, 0, 1, true);
		////Dxl.moveServo(2, 15, true, true);
		
		cout << "To run centerTrackedWormToPoint " << endl;



		centerTrackedWormToPoint(worms, Ox, worms.high_mag_fov_center_loc);

	}

	cout << "			Done centering worm: " << time_steps.getElapsedTime() << endl;
	time_steps.setToZero();

	// If aborted, don't do any followup steps
	if (abort_lowering) {
		abort_lowering = false;
		cout << "ABORT 5" << endl;

		return false;
	}


	/*
		STEP III: Finally, X and Y are centered, but STILL not touching? lower vertically. Save center position
					Then, sweep and raise up.
	*/

	Err.notify(PRINT_ONLY, "STEP III: Vertical lower if not touching.");

	// Tilting the pick if dropping the worm
	//if (sweep < 0)
	//{
	//	Dxl.moveServo(3, 8, true, true);
	//	Dxl.moveServo(2, 8, true, true);
	//	cout << "Tilting pick" << endl;

	//	//boost_sleep_ms(1000);
	//	// real_pt = pick_tip;
	//}


	// Move the pick back a bit so we can see the worm dropping
	if(sweep < 0){ Dxl.moveServo(2, 18, true, true); }
	
	cout << "			Done moving drop pick into view: " << time_steps.getElapsedTime() << endl;
	time_steps.setToZero();

	//powerCycleCapSensor(Grbl2.getLabJackHandle());

	lowerPickToTouch(Grbl2, Grbl2.getLabJackHandle(), true);

	cout << "			Done lower pick to touch: " << time_steps.getElapsedTime() << endl;
	time_steps.setToZero();

	Err.notify(PRINT_ONLY, "\tfocus_height changed to: " + to_string(focus_height));


	// Allow user to abort without trying again
	if (abort_lowering) {
		abort_lowering = false;
		cout << "ABORT 6" << endl;

		return false;
	}

	// Optionally add an extra vertical dig to push deeper into the agar
	moveServo(1, dig, true, true, false);
	Err.notify(PRINT_ONLY, "\tDig servo 1 by " + to_string(dig));


	// record the last seen tip position
	real_pt = pick_tip;

	// Wait a moment to let worm stick
	// boost_sleep_ms(250);

	// Alow user to abort without trying again
	if (abort_lowering) {
		abort_lowering = false;
		cout << "ABORT 7" << endl;

		return false;
	}


	if (sweep < 0)
	{
		boost_sleep_ms(pause * 1000);// Optionally wait before the sweep
	}

	// Optionally sweep the pick in y
	Err.notify(PRINT_ONLY, "\tSweeping increments: " + to_string(sweep));
	for (int i = 0; i < abs(sweep); i++) {
		moveServo(2, 1 * sweep / abs(sweep), true, true);
		boost_sleep_ms(0);
	}


	// Shaking action for worm dropping
	/*double direction = 1;
	if (sweep < 0) {
		for (int i = 0; i < 10; i++) {
			moveServo(2, 5 * direction, true, true);
			direction = -direction;
			boost_sleep_ms(500);
		}
	}*/


	// Optionally wait after the sweep 
	boost_sleep_ms(pause * 1000);

	// Sweep pick horizontal and vertical to drop worm
	if (sweep < 0) {
		for (int i = 0; i < 8; i++) {
			Ox.goToRelative(0.1, 0, 0, 1, false, 0);
			boost_sleep_ms(50);
		}


		for (int i = 0; i < abs(sweep) * 0.3; i++) {
			moveServo(2, - 1 * sweep / abs(sweep), true, true);
			boost_sleep_ms(40);
		}

	}

	// Raise pick
	Err.notify(PRINT_ONLY, "\tMoving pick away...");
	if (sweep > 0 ) {
		VmovePickSlightlyAway(-1, 0, -1, true); // Input arguments: (stepsz1-3) step size of motor 1-3. (0 means disabled, not move)
										  //stepsz 1&3 < 0; stepsz 2 > 0;
	}
	else {
		movePickSlightlyAbovePlateHeight();
		//movePickSlightlyAway();
	}

	cout << "			Done raising pick: " << time_steps.getElapsedTime() << endl;
	time_steps.setToZero();

	// Make sure continue_lowering is off (should be)
	continue_lowering = false;
	Err.notify(PRINT_ONLY, "\tCompleted STEP III.");

	// Reset pickup location
	worms.setPickupLocation(orig_pickup_pt.x, orig_pickup_pt.y);

	// Lower down the grbl2
	// Grbl2.goToAbsolute(Grbl2.getX(), Grbl2.getY(), Grbl2.getZ() + pick_focus_height_offset, true, 0);
	
	//// Go back to original position
	//if (sweep < 0) {
	//	Ox.goToRelative(-0.8, 0, 0, 1, false, 0);
	//}

	// Verify the pick-up or unloading success

	AnthonysTimer Tv;

	if (selectionMode == AUTO)
	{
		worms.setPickupLocation(real_pt.x, real_pt.y);
		// TODO: Need to set it up to use ParalWormFinder to see if the worm crawled away from the pick, right now I'm just setting the bool to true for the interim.
		//bool AnyWrmCrwlOut = worms.WrmCrwlOutOfPk(); // Check any worm crawl out from pick
		bool AnyWrmCrwlOut = true;
		cout << "Is worm unloaded? " << AnyWrmCrwlOut << endl;

		worms.setPickupLocation(orig_pickup_pt.x, orig_pickup_pt.y);

		bool success = false;
		if (sweep > 0) { success = !AnyWrmCrwlOut; }
		if (sweep < 0) { success = AnyWrmCrwlOut; }

		cout << "Verification time ";
		Tv.printElapsedTime();

		return success;
	}


	worms.worm_finding_mode = worms.WAIT;

	Err.notify(PRINT_ONLY, "\tReady to exit picking action.");

	return true;


	// Write the pick deviation distance and picking result to file
	cout << "Write the picking data to file..." << endl;


	// Ask write to file?
	string fout_fragment;
	cout << "Write to file?    0  -or-  ENTER...\n";
	getline(cin, fout_fragment);

	if (fout_fragment.size() == 0) {

		// Successfully pick up a worm?
		string result_fragment;
		cout << "Success?    0  -or-  1...\n";
		getline(cin, result_fragment);

		// Write to history file
		writePickHis(historyFile, target_pt, real_pt, adj_pt, result_fragment);
	}

	return true;
}

void WormPick::readyPickToStartPosition(OxCnc &Grbl2) {
	AnthonysTimer time_steps;
	pick_at_start_pos = false;
	moving_pick_to_start = true;

	int start_offset = 0;			 // -20 before change. Start with a very generous offset in case of pick bends

	// If we're heating the pick then we first wait for it to finish before getting the pick into position.
	while (heating_pick) {
		boost_sleep_ms(20);
	}

	// Power cycle the capacitive sensor before we move the pick into position. This way we know the sensor is calibrated
	// before touching the agar and we can get good readings when we lower the pick.
	/*if(!cap_sensor_ready)
		powerCycleCapSensor(Grbl2.getLabJackHandle());*/


	cout << "			Speedpick start motion: " << time_steps.getElapsedTime() << endl;
	time_steps.setToZero();

	/*
		STEP I: Position the pick just above the agar and to the right of the worm.
	*/

	Err.notify(PRINT_ONLY, "STEP I - Trying to Position pick");

	//// Move Grbl right above the focus height
	//Grbl2.goToAbsolute(Grbl2.getX(), Grbl2.getY(), getPickupHeight() - pick_focus_height_offset, true, 0);
	if (!movePickToStartPos(Grbl2, 1, 0, 0, 0))
		cout << "Failed to move pick to the start position" << endl;

	cout << "			Done go to right above focus height: " << time_steps.getElapsedTime() << endl;
	time_steps.setToZero();


	//// Use non careful mode to move the pick to start position
	//if (!movePickToStartPos(Grbl2, 0, 0, start_offset, 0)) {
	//	cout << "Failed to move pick to the start position" << endl;
	//}

	cout << "			Done move to start position (non careful): " << time_steps.getElapsedTime() << endl;
	time_steps.setToZero();
	
	moving_pick_to_start = false;
	pick_at_start_pos = true;
}

void WormPick::readyPickToStartPositionThread(OxCnc &Grbl2, WormPick &Pk) {
	boost::thread ready_pick_thread = boost::thread(&WormPick::readyPickToStartPosition, &Pk, boost::ref(Grbl2));
}


/*
Method for maneuvering the worm pick to perform either a picking action or a dropping action.
Will return the number of worms found using verifyPickOrDrop method (right now this is only being performed when dropping a worm)
*/
int WormPick::pickOrDropOneWorm(OxCnc& Ox, OxCnc &Grbl2, WormFinder& worms,
	int dig,
	int sweep,
	double pause,
	bool restore_height_after,
	bool integrate_worm_centering,
	pick_mode selectionMode,
	bool verify_drop_over_whole_plate,
	string plate_id,
	int num_expected_worms)
{

	Err.notify(PRINT_ONLY, "\n\n---------- Starting quickPick() ----------\n");

	AnthonysTimer time_steps;

	// If the pick is currently moving to the start position then wait for it to finish
	while (moving_pick_to_start) {
		boost_sleep_ms(20);
	}

	cout << "Done waiting for pick to move to start (Only when going to source): " << time_steps.getElapsedTime() << endl;
	time_steps.setToZero();

	// Move the pick to the start position if it has not been already positioned. If we are going to try to center the worm we will ready the pick after.
	if (!pick_at_start_pos && !integrate_worm_centering) {
		// Move Grbl right above the focus height
		//cout << "	Move Grbl right above focus on dest plate: " << time_steps.getElapsedTime();
		//Grbl2.goToAbsolute(Grbl2.getX(), Grbl2.getY(), getPickupHeight() - 2, true, 0); // 0.8 secs
		cout << "	Going to start pos on dest plate: " << time_steps.getElapsedTime();
		readyPickToStartPosition(Grbl2); // --PowerCycle 0.4sec -- Move pick 0.28s 
	}
	
	cout << "Done moving pick to start (Only going to dest): " << time_steps.getElapsedTime() << endl;
	time_steps.setToZero();

	// Completed step 1!
	Err.notify(PRINT_ONLY, "\tSTEP I complete. Ready for final drop");

	// Allow user to manually abort if something goes wrong with motion
	if (abort_lowering) {
		abort_lowering = false;
		cout << "ABORT 1" << endl;

		return -1;
	}

	/*
	STEP II (optional):
		If we're using offset lowering mode and the pick isn't touching yet,
		the worm had a chance to crawl away.
		Re-center worm before vertical lowering (don't raise the pick up).
	*/

	Err.notify(PRINT_ONLY, "STEP II - Re-centering the worm if desired");

	//if (integrate_worm_centering && sweep > 0) {
	if (integrate_worm_centering) {
		// Keep the worm centered at the high magnification fov
		// This is useful if the worm has crawled around while getting the pick positioned. We can re-center prior to
		// The final lowering and pick action.
		movePickSlightlyAway();
		boost_sleep_ms(50);
		cout << "Attempting to center the worm one last time before picking..." << endl;
		centerTrackedWormToPoint(worms, Ox, worms.high_mag_fov_center_loc, worms.TRACK_ONE, false, false);
		readyPickToStartPosition(Grbl2);
	}

	cout << "			Done centering worm: " << time_steps.getElapsedTime() << endl;
	time_steps.setToZero();

	/*
		STEP III: Finally, X and Y are centered, but STILL not touching? lower vertically. Save center position
					Then, sweep and raise up.
	*/

	Err.notify(PRINT_ONLY, "STEP III: Vertical lower if not touching.");

	// Move the pick back a bit so we can see the worm dropping
	if (sweep < 0) { Dxl.moveServo(2, 10, true, true); }

	// Incrementally lower the pick until it registers a touch
	lowerPickToTouch(Grbl2, Grbl2.getLabJackHandle(), true, plate_id);

	cout << "			Done lower pick to touch: " << time_steps.getElapsedTime() << endl;
	time_steps.setToZero();

	// Allow user to abort without trying again
	if (abort_lowering) {
		abort_lowering = false;
		cout << "ABORT 2" << endl;

		return -1;
	}

	// Optionally add an extra vertical dig to push deeper into the agar
	moveServo(1, dig, true, true, false);
	Err.notify(PRINT_ONLY, "\tDig servo 1 by " + to_string(dig));

	// If dropping a worm then we can optionally wait before the sweep
	if (sweep < 0)
	{
		boost_sleep_ms(pause * 500); // We do half the specified wait time before the sweep
	}

	// Optionally sweep the pick in y
	Err.notify(PRINT_ONLY, "\tSweeping increments: " + to_string(sweep));
	for (int i = 0; i < abs(sweep); i++) {
		moveServo(2, 1 * sweep / abs(sweep), true, true);
		boost_sleep_ms(0);
	}

	// Optionally wait after the sweep 
	boost_sleep_ms(pause * 1000);

	// Sweep pick horizontal and vertical to drop worm
	if (sweep < 0) {

		// Sweep horizontally to encourage worm stuck on inside of loop to crawl off
		/*for (int i = 0; i < 4; i++) {
			Ox.goToRelative(0.1, 0, 0, 1, false, 0);
			boost_sleep_ms(10);
		}*/
		Ox.goToRelative(0.4, 0, 0, 1, false, 0);

		//for (int i = 0; i < 2; i++) {
		//	//moveServo(2, -1 * sweep / abs(sweep), true, true);
		//	moveServo(2, 1 , true, true);
		//	boost_sleep_ms(5);
		//}

		// Sweep back down vertically again to encourage worm on the inside of the loop to crawl off.
		for (int i = 0; i < abs(sweep); i++) {
			moveServo(2, -1 * sweep / (2 * abs(sweep)), true, true);
			boost_sleep_ms(0);
		}

	}

	// Raise pick
	Err.notify(PRINT_ONLY, "\tMoving pick away...");
	//if (sweep > 0) {
	//	// Sweep the pick in an arc if picking up a worm
	//	VmovePickSlightlyAway(-1, 0, -1, true); // Input arguments: (stepsz1-3) step size of motor 1-3. (0 means disabled, not move)
	//									  //stepsz 1&3 < 0; stepsz 2 > 0;
	//}
	//else {
	//	// Raise the pick directly upwards if dropping a worm
	//	//movePickSlightlyAbovePlateHeight();
	//	movePickAway(); // Clear the pick from view for drop verification
	//	boost_sleep_ms(20);
	//	
	//}

	if (!movePickAway()) exit(EXIT_FAILURE); // We must move the pick fully out of view because we must next either perform drop verification, or laser focus at a drop site.
	if(sweep < 0)
		Ox.goToRelative(-0.4, 0, 0, 1, false, 0);

	cout << "			Done raising pick: " << time_steps.getElapsedTime() << endl;
	cout << "^^^^^^^^^^^^^^^^^^^^ AIN0 after raising: " << LabJack_ReadAINN(Grbl2.getLabJackHandle(), 0) << endl;
	time_steps.setToZero();

	// Make sure continue_lowering is off (should be)
	continue_lowering = false;
	Err.notify(PRINT_ONLY, "\tCompleted STEP III.");
	
	int verified_worms = 0; // If we're not dropping then we will not do any verification, so we can just return 0 at the end
	if (sweep < 0) {
		// If dropping a worm then we must verify that we dropped it
		verified_worms = worms.VerifyWormPickOrDrop(num_expected_worms, verify_drop_over_whole_plate);
		cout << "Time after drop verification: " << worms.image_timer.getElapsedTime() << endl;
	}

	// We're done picking (and therefore tracking) the worm, so set the finding mode back to wait
	worms.worm_finding_mode = worms.WAIT;

	Err.notify(PRINT_ONLY, "\tReady to exit picking action.");

	pick_at_start_pos = false;  // We've picked or dropped a worm so we're no longer at the start position. 
	//cap_sensor_ready = false;

	return verified_worms;

}

//bool WormPick::VquickPick(OxCnc& Ox, OxCnc &Grbl2, WormFinder& worms,
//																		int dig,
//																		int sweep,
//																		double pause,
//																		bool restore_height_after,
//																		bool integrate_worm_centering,
//																		pick_mode selectionMode)
//{
//	Err.notify(PRINT_ONLY, "\n\n---------- Starting quickPick() ----------\n");
//
//	// Reset the user's judgement and wait a moment to ensure a gap in the file 
//	action_success_user = -1;
//	boost_sleep_ms(500);
//
//	// Store pickup parameters in case changes aren't meant to learn/stick on this operation
//	double focus_height_bak = focus_height;
//	action_success = false; /// set successful pick/drop to false. Will be checked later.
//
//	// Allow several tries in case of failures
//	int tries = 0;
//	bool success_positioned = false; // Whether or not the pick is successfully positioned ready for the final lowering
//	int start_offset = -20;			 // Start with a very generous offset in case of pick bends
//
//	// Set X lowering position
//	double x_lower_rel = 0.63;
//	double y_lower_rel = 0.40;
//
//	// Keep track of whether the pick is currently directly above the worm or offset
//	bool worm_offset_status = 1; /// Centering moves worm to the offset position
//
//	while (++tries < 4 && !success_positioned) {
//
//		// Alow user to abort without trying again
//		if (abort_lowering) {
//			abort_lowering = false;
//			cout << "ABORT 1" << endl;
//			return false;
//		}
//
//		/*
//			STEP I: Position the pick just above the agar and to the right of the worm.
//		*/
//
//		Err.notify(PRINT_ONLY, "STEP I - Try " + to_string(tries) + " - Position pick");
//
//		// Verify that the pick is at the focus height
//		Grbl2.goToAbsolute(Grbl2.getX(), Grbl2.getY(), getPickupHeight(), true, 20);
//
//		// Power cycle the capacitive sensor so we can check for touch
//		powerCycleCapSensor(Grbl2.getLabJackHandle());
//		continue_lowering = true;
//
//
//		// Move pick to "start" location, everything in place to touch EXCEPT servo 1 slightly up
//		//		e.g. move servos only, with an offset on serv1, without grbl (grbl would send 
//		//		it up in Z to the true start position)
//		if (!movePickToStartPos(Grbl2, 0, 0, start_offset))
//			continue;
//
//		/*
//			STEP I-a: [Optional] Position the worm 1 dydOx unit below center point
//		*/
//		AnthonysTimer Ta;
//		if (integrate_worm_centering) {
//
//			/// If we aren't offset, then go to the offset position and re-find the worm
//			if (!worm_offset_status) {
//				worm_offset_status = Ox.goToRelative(0, -1, 0, 1, true);
//				boost_sleep_ms(150);
//				if (!worms.findWormNearestPickupLoc()) {
//					return false;
//				}
//
//			}
//
//			/// Verify that the worm is centered at the offset position (5s time limit)
//			helperCenterWorm(Ta, 5, worms, Ox);
//		}
//
//
//		// Center pick in Y
//		AnthonysTimer Ty;
//		boost_sleep_ms(200);
//		while (!fineTuneCentering(x_lower_rel, y_lower_rel, false, true) && Ty.getElapsedTime() < 5) {}
//
//		pick_center_2 = getServoPos(2); /// New Y center position, esp if going to have trouble later.
//
//		Err.notify(PRINT_ONLY, "\ty-center (d2) changed to: " + to_string(pick_center_2));
//
//		// Exception 1: The pick touched AND:
//			// it is left of the center, AND
//			// We're still on the first try, OR
//			// Pick is far left (regardless of whether it's touching).
//			// Increase the servo 1 position
//		if ((!continue_lowering && pick_tip_visible && pick_tip_rel.x < x_lower_rel - 0.02 && tries < 2) || pick_tip_rel.x < x_lower_rel - 0.05) {
//			Err.notify(PRINT_ONLY, "\tE1 in STEP I - try " + to_string(tries));
//
//			// Raise servo 1
//			pick_start_1 -= 5;
//			movePickToStartPos(Grbl2, 0, start_offset); /// Move immediately before can reset continue_lowering
//			Err.notify(PRINT_ONLY, "\tpick_start_1 changed to: " + to_string(pick_start_1));
//
//			boost_sleep_ms(250);
//			continue_lowering = true;
//			continue;
//
//		}
//
//		// Exception 2: The pick touched and is actually at the right position (by accident...)
//			// Continue to final lowering, which should be automatically skipped by continue_lowering = false.
//		else if (!continue_lowering && pick_tip_visible &&
//			pick_tip_rel.x < x_lower_rel &&
//			pick_tip_rel.x > x_lower_rel - 0.1)
//		{
//			Err.notify(PRINT_ONLY, "\tE2 in STEP I - continuing");
//			/// Do nothing, just proceed to step II
//		}
//
//		// Exception 3: Pick touched but we either can't see or or it's on the right or we already tried once. Raise Z axis. 
//		else if (!continue_lowering) {
//			Err.notify(PRINT_ONLY, "\tE3 in STEP I - try " + to_string(tries));
//
//			//Raise Z
//			focus_height = getPickupHeight() - 3;
//			Grbl2.goToAbsolute(Grbl2.getX(), Grbl2.getY(), focus_height, true, 20);
//			boost_sleep_ms(500);
//			Err.notify(PRINT_ONLY, "\tfocus_height changed to: " + to_string(focus_height));
//
//			// Continue lowering needs to be re-set to true. The autoscanner can only set it to false, not reset to true
//			continue_lowering = true;
//			continue;
//		}
//
//		// Exception 4: The tip is not visible. Need to re-center
//		else if (!pick_tip_visible) {
//
//			Err.notify(PRINT_ONLY, "E4 in STEP I - need to center");
//			continue_lowering = false;
//			return false;
//		}
//
//		// Otherwise, completed step 1!
//		else {
//			Err.notify(PRINT_ONLY, "\tSTEP I complete. Ready for final drop");
//		}
//
//		// Allow user to abort without trying again
//		if (abort_lowering) {
//			abort_lowering = false;
//			cout << "ABORT 2" << endl;
//
//			return false;
//		}
//
//		/*
//			STEP Ia (optional): confirm worm is still centered at the offset point and eliminate the offset
//		*/
//
//		if (integrate_worm_centering && worm_offset_status) {
//			worm_offset_status = !Ox.goToRelative(0, 1, 0, 1.0, true, 5);
//			boost_sleep_ms(150);
//			fineTuneCentering(false, true); /// Step pick Y centering in case of vibrations moving it around
//		}
//
//		// Alow user to abort without trying again
//		if (abort_lowering) {
//			abort_lowering = false;
//			cout << "ABORT 3" << endl;
//
//			return false;
//		}
//
//		/*
//			STEP II: Final lowering of the pick
//		*/
//
//		double step2buff = 0.05;
//		Err.notify(PRINT_ONLY, "STEP II - Try " + to_string(tries) + " - Final lower");
//
//		// If pick looks good, perform final dip into the agar SLOWLY (start pos with NO offset on servo1)
//		// Stop if:
//		//		- We've complete the entire final dip, OR
//		//		- The tip is no longer visible, OR
//		//		- The tip has touched the surface (continue lowering is 0), OR
//		//		- The tip has moved too far left (remaining lowering must be done vertically from the center position). 
//
//		if (pick_tip_rel.y > y_lower_rel - 0.1 && pick_tip_rel.y < y_lower_rel + 0.1 && pick_tip_visible) {
//			Err.notify(PRINT_ONLY, "\tCentered and visible, perform final lowering...");
//
//			for (int i = 0;
//				i < abs(start_offset * 2) && pick_tip_visible && continue_lowering && pick_tip_rel.x > x_lower_rel;
//				i++)
//			{
//				// Lower by 1 step
//				moveServo(1, 1, true, true);
//				boost_sleep_ms(100);
//
//				// If Y has gone OOB, adjust it
//				fineTuneCentering(x_lower_rel, y_lower_rel, false, true);
//			}
//		}
//
//		// Exception 5: Can't see the pick anymore
//		else {
//			Err.notify(PRINT_ONLY, "\tE5 in STEP II - retrying");
//			continue;
//		}
//
//		// Exception 6: Pick hit the ground before reaching center. If so move up and try again.
//		boost_sleep_ms(150);
//		if (!continue_lowering && pick_tip_rel.x > x_lower_rel) {
//			Err.notify(PRINT_ONLY, "\tE6 in STEP II - try " + to_string(tries));
//			boost_sleep_ms(50);
//			focus_height = getPickupHeight() - 1.5;
//			continue;
//		}
//
//		// Exception 7: Pick lowered all the way but did not hit ground OR center. Decrease starting angle
//		if (continue_lowering && pick_tip_rel.x > x_lower_rel + step2buff) {
//			Err.notify(PRINT_ONLY, "\tE7 in STEP II - try " + to_string(tries));
//			boost_sleep_ms(50);
//			pick_start_1 += 5;
//			continue;
//		}
//
//		// One last attempt to center Y, assuming it isn't touching yet. 
//		if (continue_lowering)
//			fineTuneCentering(false, true);
//
//		// Did we succeed? centered?
//		success_positioned = pick_tip_rel.x > x_lower_rel - step2buff &&
//			pick_tip_rel.x < x_lower_rel + step2buff &&
//			pick_tip_rel.y > y_lower_rel - step2buff &&
//			pick_tip_rel.y < y_lower_rel + step2buff;
//
//		Err.notify(PRINT_ONLY, "\tSTEP 2 succeeded (1/0)? - " + to_string(success_positioned));
//
//
//	}
//
//	// If we tried 3 times and still couldn't position properly, exit failure
//	if (!success_positioned) {
//		Err.notify(PRINT_ONLY, "\tGAVE UP.");
//		return false;
//	}
//
//	// Alow user to abort without trying again
//	if (abort_lowering) {
//		abort_lowering = false;
//		cout << "ABORT 4" << endl;
//
//		return false;
//	}
//
//	/*
//		STEP IIa (optional):
//			If we're using offset lowering mode and the pick isn't touching yet,
//			the worm had a chance to crawl away.
//			Re-center worm before vertical lowering (don't raise the pick up).
//	*/
//
//
//	if (integrate_worm_centering && continue_lowering) {
//		Err.notify(PRINT_ONLY, "\tRunning StepIIa offset");
//		/// If we aren't offset, then go to the offset position and re-find the worm
//		if (!worm_offset_status) {
//			worm_offset_status = Ox.goToRelative(0, -1, 0, 1, true);
//			boost_sleep_ms(150);
//			if (!worms.findWormNearestPickupLoc())
//				cout << "findWormNearestPickupLoc fail" << endl;
//				return false;
//		}
//
//		/// Verify that the worm is centered at the offset position (5s time limit)
//		AnthonysTimer Ta;
//		helperCenterWorm(Ta, 5, worms, Ox);
//
//		/// Move the pick back over the worm to allow lowering correctly
//		worm_offset_status = Ox.goToRelative(0, 1, 0, 1, true);
//
//	}
//
//	// If aborted, don't do any followup steps
//	if (abort_lowering) {
//		abort_lowering = false;
//		cout << "ABORT 5" << endl;
//
//		return false;
//	}
//
//
//
//	/*
//			STEP III: Finally, X and Y are centered, but STILL not touching? lower vertically. Save center position
//						Then, sweep and raise up.
//	*/
//	Err.notify(PRINT_ONLY, "STEP III: Vertical lower if not touching.");
//	lowerPickToTouch(Grbl2, Grbl2.getLabJackHandle(), false);
//	Err.notify(PRINT_ONLY, "\tfocus_height changed to: " + to_string(focus_height));
//
//
//	// Allow user to abort without trying again
//	if (abort_lowering) {
//		abort_lowering = false;
//		cout << "ABORT 6" << endl;
//
//		return false;
//	}
//	else if (!continue_lowering) {
//		setPickCenter(Dxl.positionOfServo(1), Dxl.positionOfServo(2), Dxl.positionOfServo(3), Grbl2.getY(), Grbl2.getZ() - 5, Grbl2.getZ());
//	}
//
//	// Optionally add an extra vertical dig to push deeper into the agar
//	moveServo(1, dig, true, true, false);
//	Err.notify(PRINT_ONLY, "\tDig servo 1 by " + to_string(dig));
//
//	// Wait a moment to let worm stick
//	boost_sleep_ms(250);
//
//	// Alow user to abort without trying again
//	if (abort_lowering) {
//		abort_lowering = false;
//		cout << "ABORT 7" << endl;
//
//		return false;
//	}
//
//	// Optionally sweep the pick in y
//	Err.notify(PRINT_ONLY, "\tSweeping increments: " + to_string(sweep));
//	for (int i = 0; i < abs(sweep); i++) {
//		moveServo(2, 1 * sweep / abs(sweep), true, true);
//		boost_sleep_ms(75);
//	}
//
//	// Optionally wait after the sweep 
//	boost_sleep_ms(pause * 1000);
//
//	// Raise pick
//	Err.notify(PRINT_ONLY, "\tMoving pick away...");
//
//	if (sweep > 0)
//		VmovePickSlightlyAway(-1, 0, -1); // Input arguments: (stepsz1-3) step size of motor 1-3. (0 means disabled, not move)
//											//stepsz 1&3 < 0; stepsz 2 > 0;
//	if (sweep < 0)
//		VmovePickSlightlyAway(1, 0, 1);
//
//	// Make sure continue_lowering is off (should be)
//	continue_lowering = false;
//	Err.notify(PRINT_ONLY, "\tCompleted STEP III.");
//
//	// If height learning / adapting is to be blocked, restore the old focus height values
//	if (restore_height_after) {
//		focus_height = focus_height_bak;
//		Err.notify(PRINT_ONLY, "\tRestored original focus_height.");
//	}
//
//
//	// If in DROP mode, wait a bit to check that the worm is off
//	if (selectionMode == DROP) {
//		if (worms.wasWormDropped()) {
//			Err.notify(PRINT_ONLY, "successfully dropped worm\n");
//		}
//		else {
//			Err.notify(PRINT_ONLY, "failed to drop worm\n");
//			// TODO try again?
//		}
//	}
//
//	//If in AUTO pick mode, verify the worm was picked
//	if (selectionMode == AUTO) {
//		if (!worms.mvmtAtPickedSite()) {
//			worms.setPickedWorm();
//			action_success = true;
//			Err.notify(PRINT_ONLY, "successfully picked worm\n");
//		}
//		else {
//			Err.notify(PRINT_ONLY, "failed to pick worm\n");
//			action_success = false;
//			return false;
//		}
//	}
//
//	// Clear abort flag if reached here and it's still on
//	if (abort_lowering) {
//		abort_lowering = false;
//		cout << "ABORT 8" << endl;
//
//		return false;
//	}
//	else {
//
//
//		return true;
//	}
//}


bool WormPick::calibratePick(OxCnc& Grbl2, int range, int step_sz) {

	movePickToStartPos(Grbl2, true, true, 0, false);

	// Compute the vector of motor positions for calibration

	setcalibr_servo_coords(pick_start_1, pick_center_2, pick_center_3, range, step_sz);

	// TO DO: check out of bound condition

	vector<Point> tip_positions;
	vector<Point> tip_positions_conv; // tip positions computed by conventional machine vision
	// Move the servo motors to each position
	for (int i = 0; i < calibr_servo_coords.size(); i++) {

		Dxl.moveServo(1, calibr_servo_coords[i].x, false, false, false);
		Dxl.moveServo(2, calibr_servo_coords[i].y, false, false, false);
		Dxl.moveServo(3, calibr_servo_coords[i].z, false, false, false);

		cout << "Servo moves to (" << calibr_servo_coords[i].x << ", " << calibr_servo_coords[i].y << ", " << calibr_servo_coords[i].z << ")" << endl;
		
		//Wait to be stable
		boost_sleep_ms(1300);

		// Record the tip position
		tip_positions.push_back(pick_tip);

		tip_positions_conv.push_back(pick_tip_conv);

		cout << "Tip position by CNN = (" << tip_positions[i].x << ", " << tip_positions[i].y << ")" << endl;
		cout << "Tip position by conventional methods = (" << tip_positions_conv[i].x << ", " << tip_positions_conv[i].y << ")" << endl;

	}

	// Save the tip position to calibr_tip_coords
	setcalibr_tip_coords(tip_positions);

	// Make a map
	settip_servo_map(calibr_tip_coords, tip_positions_conv, calibr_servo_coords, true);

	// Move pick slightly away
	movePickSlightlyAway();

	return true;
}

bool WormPick::compareDist(std::pair<cv::Point, cv::Point3i> a, std::pair<cv::Point, cv::Point3i> b) {
	return (ptDist(a.first, target_pt) < ptDist(b.first, target_pt));
}


void WormPick::writePickHis(const string hFile, const cv::Point target_pos, const cv::Point real_pos, const cv::Point corrected_pos, const string success) {

	cout << "Write the pick deviation data to file..." << endl;

	ofstream DevData;
	DevData.open(hFile, std::fstream::app);

	if (DevData.is_open()) {
		DevData << target_pos.x << "\t" << target_pos.y << "\t" << real_pos.x << "\t" << real_pos.y << "\t";
		DevData << corrected_pos.x << "\t" << corrected_pos.y << "\t";
		DevData << success;
		DevData << endl;
		DevData.close();
	}
	else {
		cout << "Cannot open pick deviation data file!" << endl;
	}
}

void WormPick::getPickAdjHist(int& cx, int& cy, int& cz, const string& hFile, int len_his) {

	// Compute adjustment term from history file
	// len_his: length of history, i.e. the number of picking events used to compute the adjustment term

	string line;
	ifstream hist; // History file
	hist.open(hFile, std::fstream::in);
	vector<Point> target_pos;
	vector<Point> real_pos;
	vector<int> Dev_x, Dev_y;

	if (hist.is_open()) {

		int a; int len = 7; vector<int> line;
		while (hist >> a)
		{
			// printf("%i", a);
			len = len - 1;
			line.push_back(a);
			if (len == 0) {

				target_pos.push_back(Point(line[0], line[1]));
				real_pos.push_back(Point(line[2], line[3]));

				line.clear();
				len = 7;
			}
		}
		
		// compute the average deviation in pick position by the history
		if (target_pos.size() == real_pos.size() && target_pos.size() >= len_his) {
			for (int i = target_pos.size() - 1; (i >= (target_pos.size() - len_his) && i >= 0); i--) {
				cout << i << endl;
				cout << "Dev_x = " << real_pos[i].x - target_pos[i].x  << endl;
				Dev_x.push_back(real_pos[i].x - target_pos[i].x);
				Dev_y.push_back(real_pos[i].y - target_pos[i].y);
			}

			double mean_dev_x = vectorMean(Dev_x);
			double mean_dev_y = vectorMean(Dev_y);

			// Calculate the pick compensation movement
			cx = -mean_dev_x / dxd1;
			cy = -mean_dev_y / dyd2;
			cz = 0;


			cout << "cx = " << cx << endl;
			cout << "cy = " << cy << endl;

		}
		else {
			cout << "No enough history!" << endl;
			// getPickAdjCurFrm(cx, cy, cz, 2, 1);
		}

	}
	else { cout << "Unable to read from the pick history file!" << endl;}

}

void WormPick::getPickAdjCurFrm(int& cx, int& cy, int& cz, double multi_x, double multi_y, double multi_z) {

	cx = (-(real_pt.x - target_pt.x) / dxd1) * multi_x;
	cy = (-(real_pt.y - target_pt.y) / dyd2) * multi_y;
	cz = 0;

	cout << "cx = " << cx << endl;
	cout << "cy = " << cy << endl;
}


void WormPick::updateCaliMapBound() {

	if (!tip_servo_map.empty()) {

		// Find the extreme values of x & y in the map
		int min_x_idx = 0, min_y_idx = 0, max_x_idx = 0, max_y_idx = 0;

		for (int i = 0; i < tip_servo_map.size(); i++) {
			if (tip_servo_map[min_x_idx].first.x > tip_servo_map[i].first.x) {
				min_x_idx = i;
			}

			if (tip_servo_map[min_y_idx].first.y > tip_servo_map[i].first.y) {
				min_y_idx = i;
			}

			if (tip_servo_map[max_x_idx].first.x < tip_servo_map[i].first.x) {
				max_x_idx = i;
			}

			if (tip_servo_map[max_y_idx].first.y < tip_servo_map[i].first.y) {
				max_y_idx = i;
			}
		}

		min_x = tip_servo_map[min_x_idx].first.x;
		min_y = tip_servo_map[min_y_idx].first.y;
		max_x = tip_servo_map[max_x_idx].first.x;
		max_y = tip_servo_map[max_y_idx].first.y;

		// Renew dxd1 & dyd2;
		dxd1 = (min_x - max_x) / (tip_servo_map[min_x_idx].second.x - tip_servo_map[max_x_idx].second.x);
		dyd2 = (max_y - min_y) / (tip_servo_map[max_y_idx].second.y - tip_servo_map[min_y_idx].second.y);


	}
	else {
		min_x = -1; min_y = -1; max_x = -1; max_y = -1;
		cout << "The calibration map is empty!" << endl;
	}

}

bool WormPick::withinCalibrMap(const cv::Point targ_pt) {
	if (min_x != -1 && min_y != -1 && max_x != -1 && max_y != -1) {

		// Check whether the target point is in bound
		return (targ_pt.x >= min_x && targ_pt.x <= max_x && targ_pt.y >= min_y && targ_pt.y <= max_y);
	}
	else {
		cout << "The calibration boundary is undefined!" << endl;
		return false;
	}

}


bool WormPick::CaliMapfromfile(const string pFile) {

	tip_servo_map.clear();

	string line;
	ifstream map;
	map.open(pFile, std::fstream::in);

	if (map.is_open()) {

		int a; int len = 5; vector<int> line;
		while (map >> a)
		{
			// printf("%i", a);
			len = len - 1;
			line.push_back(a);
			if (len == 0) {
				pair<Point, Point3i> element(Point(line[0], line[1]), Point3i(line[2], line[3], line[4]));
				tip_servo_map.push_back(element);
				line.clear();
				len = 5;
			}
		}
		// getchar();

		// Update the boundary of calibration region
		updateCaliMapBound();

		return true;
	}
	else { cout << "Unable to read from the pick calibration file!" << endl; return false; }
}

bool WormPick::MovePickbyCalibrMap(OxCnc& Grbl2, const cv::Point targ_pt, const string& history) {

	// load the calibration data from file if the map is empty
	if (tip_servo_map.empty()) {
		cout << "Fail to move the pick! The calibration map is empty!" << endl;
		movePickSlightlyAway();
		return false;
	}

	// Find the point cloest to the target point in the map (tip_servo_map)
	target_pt = targ_pt;

	int min_idx = 0;
	for (int i = 0; i < tip_servo_map.size(); i++) {
		if (ptDist(tip_servo_map[min_idx].first, target_pt) > ptDist(tip_servo_map[i].first, target_pt)) {
			min_idx = i;
		}
	}

	// Move to the target point
	cout << "cloest point in map: x = " << tip_servo_map[min_idx].first.x << ", y = " << tip_servo_map[min_idx].first.y << endl;
	Dxl.moveServo(1, tip_servo_map[min_idx].second.x, false, false, false);
	Dxl.moveServo(2, tip_servo_map[min_idx].second.y, false, false, false);
	Dxl.moveServo(3, tip_servo_map[min_idx].second.z, false, false, false);

	// boost_sleep_ms(800);
	real_pt = pick_tip;
	cout << "Pick position (before correction): x = " << real_pt.x << ", y = " << real_pt.y << endl;

	// Compute compensation term obtained from history (due to the decoorelation with the calibration data)
	int cx = 0, cy = 0, cz = 0;
	getPickAdjHist(cx, cy, cz, history, 10);
	

	// Move add an compensation term to pick
	if (cx!=0 || cy!=0) {
		Dxl.moveServo(1, tip_servo_map[min_idx].second.x + cx, false, false, false);
		Dxl.moveServo(2, tip_servo_map[min_idx].second.y + cy, false, false, false);
		Dxl.moveServo(3, tip_servo_map[min_idx].second.z + cz, false, false, false);
	}

	// boost_sleep_ms(1000);
	adj_pt = pick_tip;
	cout << "Pick position (after correction): x = " << adj_pt.x << ", y = " << adj_pt.y << endl;

	return true;

}

// Center the pick
void WormPick::centerPick(OxCnc& Ox, OxCnc& Grbl2, WormFinder& worms) {

	/// Raise gantry regardless of case
	tries = 0;
	Ox.raiseGantry();


	while (tries < 2 && !pick_tip_centered) {
		/// Case 1 (see below): Tip is visible, simply fine tune it from present position. Don't raise or reset.

		/// Case 2: Tip is not visible. Try move back to previous start location and see if it's visible. 
		if (tries == 0 && !pick_tip_visible) {
			while (Ox.isMoving()) {}
			movePickToStartPos(Grbl2);  /// Move pick to last known location
			boost_sleep_ms(100); /// Pause for a sec (in addition to waiting for movement to complete)
		}

		/// Case 3: Tip was not visible in either case. Start from scratch. 
		if (!pick_tip_visible) {

		}

		/// Case 1 or 2 - the pick tip is visible, proceed with centering and final cleanup
		if (pick_tip_visible) {

			/// If it is, raise the pick up first (coarsely) to make sure we aren't below the focus plane
			double l_i_bak = lower_increment;
			lower_increment = 1;
			raisePickToFocus(Grbl2);

			/// Lower pick to focus plane (coarsely) 
			boost_sleep_ms(500);
			lowerPickToFocus(Grbl2, Grbl2.getLabJackHandle());

			/// Center the pick
			for (int i = 0; i < 75 && !pick_tip_centered; i++) {
				fineTuneCentering();
				boost_sleep_ms(150);
			}
			lower_increment = l_i_bak;

			/// Assign pickup location using the in-focus tip
			worms.pickup_location = pick_tip;

			/// Save the start_lowering positions (should start at 5 mm above focus height) and focus height (Z pos)
			boost_sleep_ms(400);
			setPickCenter(Dxl.positionOfServo(1), Dxl.positionOfServo(2),
						  Dxl.positionOfServo(3), Grbl2.getY(), Grbl2.getZ() - 5, Grbl2.getZ());

			/// Correlate small servo movements to X and Y pixels shift in tip location
			//correlateServoToImage();

			/// Raise pick so it won't hit ground when lowering.
			movePickSlightlyAway();

			/// Lower the Gantry back to its original level and reset
			Ox.lowerGantry();
			cout << "Centering pick done\n";
			continue_centering = false;
		}
		tries += 1;
	}
}

// Local function only: Add pick tip location for correlation

bool WormPick::addPickPoint(int servo_id, int servo_rel_mvmt, vector<Point>& tip_locs) {
	moveServo(servo_id, servo_rel_mvmt, true, true, false);
	boost_sleep_ms(650);
	if (!pick_tip_visible) {
		cout << "Failed to correlate servo to image because pick tip was lost" << endl;
		return false;
	}
	tip_locs.push_back(pick_tip);
	return true;
 }

// Find out how much movement on servos 1, 2, and 3 moves the pick tip. GANTRY MUST BE RAISED
void WormPick::correlateServoToImage() {

	// Make sure the tip is centered
	if (!pick_tip_centered || !pick_tip_visible) {
		cout << "Could not correlate servo to image because tip is not centered" << endl;
		return;
	}

	vector<Point> tip_locs;

	// X direction - servo 1. Start from left and move right
	double mvmt = 7;
	if (!addPickPoint(1, mvmt, tip_locs) ||
		!addPickPoint(1,-mvmt, tip_locs) ||
		!addPickPoint(1,-mvmt, tip_locs))
	{
			return;
	}

	// Calculate conversion from points a, b, and c
	double dxd1_ab = (tip_locs[1].x - tip_locs[0].x) / -mvmt;
	double dxd1_bc = (tip_locs[2].x - tip_locs[1].x) / -mvmt;
	double dxd1_ac = (tip_locs[2].x - tip_locs[0].x) / (-2 * mvmt);
	dxd1 = (dxd1_ab + dxd1_bc + dxd1_ac) / 3.0;

	// Y direction - servo 2 (recenter x first)
	moveServo(1, mvmt, true, true, false);

	tip_locs.clear(); 

	if (!addPickPoint(2, mvmt, tip_locs) ||
		!addPickPoint(2, -mvmt, tip_locs) ||
		!addPickPoint(2, -mvmt, tip_locs))
	{
		return;
	}

	// Calculate conversion from points a, b, and c
	double dyd2_ab = (tip_locs[1].y - tip_locs[0].y) / -mvmt;
	double dyd2_bc = (tip_locs[2].y - tip_locs[1].y) / -mvmt;
	double dyd2_ac = (tip_locs[2].y - tip_locs[0].y) / (-2 * mvmt);
	dyd2 = (dyd2_ab + dyd2_bc + dyd2_ac) / 3.0;


}

// Read LabJackData -- thread functions
void WormPick::readLabJackAIN0(long lngHandle) {

	for (;;) {

		// Check value
		AIN0 = LabJack_ReadAINN(lngHandle, 0);

		/*if (AIN0 < 2.0)
			cout << "AIN0: " << AIN0 << endl;
		else
			cout << "AIN0 ----------> " << AIN0 << endl;*/
		//if (start_reading_for_touch) cout << "Capacitive reading: " << AIN0 << endl;
		// Lowering should immediately cease if AIN0 shows contact
		if (AIN0 > 2.0 && start_reading_for_touch) {
			cout << "^^^^^^^^^^^^^^^^^^^^ AIN0 from thread: " << AIN0 << endl;
			//cout << "Capacitive sensor triggered: " << AIN0 << endl;
			continue_lowering = false;
			start_reading_for_touch = false;
		}
		 
		// Wait a bit before repeating
		boost_interrupt_point();
		//boost_sleep_ms(10);
	}
}

// Checks the offset of the camera is aligned with Z axis
void WormPick::testCameraOffset(OxCnc *Grbl2, WormFinder *worms) {
	for (;;) {

		double cum_x_off = 0;
		double cum_y_off = 0;
		double this_x_off = 0;
		double this_y_off = 0;
		int N_trials = 5;

		// If the test is started...
		if (start_test && pick_tip_visible) {
			//printf("Starting %d trials...\t", N_trials);

			//... then run it 5 times
			// for (int i = 0; i < N_trials; i++) {

				/// Get the starting position
				int currentx = pick_tip.x;
				int currenty = pick_tip.y;

				/// Center red cross over starting position
				worms->pickup_location = pick_tip;

				/// Move to the test position (down) while waiting to finish
				Grbl2->goToRelative(0, 0, 20, 1.0, true, 5);

				/// Record the X and Y offsets relative to the starting positions
				boost_sleep_ms(500);
				this_x_off = abs(pick_tip.x - currentx);
				this_y_off = abs(pick_tip.y - currenty);
				cum_x_off += this_x_off;
				cum_y_off += this_y_off;
				cout << "X diff: " << this_x_off << " Y diff: " << this_y_off << endl;

				/// Return to the starting position and prepare to calculate in reverse
				currentx = pick_tip.x;
				currenty = pick_tip.y;
				worms->pickup_location = pick_tip;
				Grbl2->goToRelative(0, 0, -20, 1.0, true, 5);

				/// Calculate in reverse too
				boost_sleep_ms(500);
				this_x_off = abs(pick_tip.x - currentx);
				this_y_off = abs(pick_tip.y - currenty);
				cum_x_off += this_x_off;
				cum_y_off += this_y_off;
				cout << "X diff: " << this_x_off << " Y diff: " << this_y_off << endl;

		}
			// Stop cycling
			//start_test = false;
		//}

		boost_sleep_ms(2000);
		boost_interrupt_point();
	}
}

// Move the pick by key press
void WormPick::movePickByKey(int &keynum, OxCnc& Grbl2) {

	// Check whether user requested any manual movement of the servos. Auto movements 
	//		are irrelevant because user only allowed to move outside of script running
	Dxl.moveServoByKey(keynum);

	// Check whether the user requested that the current position be the focus height (start pos is fh - 5)
	if (keynum == keyMinus) {// As far as I can tell parameters Grbl2.getY(), Grbl2.getZ()-10, set the grbl2 center position, which is no longer used.
		setPickCenter(Dxl.positionOfServo(1), Dxl.positionOfServo(2), Dxl.positionOfServo(3), Grbl2.getY(), Grbl2.getZ()-10, Actx.readPosition());  
		keynum = -5;
		/*cout << "Set pick center: " << Dxl.positionOfServo(1) << ", " << Dxl.positionOfServo(2) 
			 << ", " << Dxl.positionOfServo(3) << ", " << Grbl2.getY() << ", " << Grbl2.getZ() << endl;*/
		cout << "Set pick center: " << Dxl.positionOfServo(1) << ", " << Dxl.positionOfServo(2)
			<< ", " << Dxl.positionOfServo(3) << ", " << Actx.readPosition() << endl;
	}
	
	// Check whether the user requested that the pick be lowered
	if (keynum == keyF5) {
		lowerPickToTouch(Grbl2, Grbl2.getLabJackHandle());
		keynum = -5;
	}

	if (keynum == keyF6) {
		// lowerPickToFocus(Grbl2, Grbl2.getLabJackHandle());
		localizeAndFocus(Grbl2, Grbl2.getLabJackHandle());
		keynum = -5;
	}

	// Check whether the user requested that the pick be moved to (or away from) the start-pick position
	if (keynum == keyF9) {
		movePickToStartPos(Grbl2,true,true,0,false);
		keynum = -5;
	}

	if (keynum == keyF10) {
		movePickSlightlyAway();
		//movePickToStartPos(Grbl2, true, false, 0, false);
		keynum = -5;
	}

	if (keynum == keyF11) {
		movePickAway();
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
// TODO: Note that Grbl is no longer needed for wormpick motion in Wormpicker 2. It can be removed from this.
bool WormPick::movePickToStartPos(OxCnc& Grbl2, bool move_actx, bool directly_to_focus_height, int servo1_offset, bool careful) {



	// Move actuator to the start position height, with offset.
	if (move_actx && directly_to_focus_height)
		Actx.moveAbsolute(focus_height);
	else if (move_actx)
		Actx.moveAbsolute(focus_height - pick_focus_height_offset);


	// Don't do anything if the pick is already at the start pos
	if (getServoPos(1) == pick_start_1 && getServoPos(2) == pick_center_2 && getServoPos(3) == pick_center_3) {
		return true;
	}

	// Move dynamixel motors to center position position while servo 1 is UP.
	//Dxl.resetServoToMin(1);
	Dxl.moveServo(2, pick_center_2, false, false, false);
	Dxl.moveServo(3, pick_center_3, false, true, false);
	//boost_sleep_ms(500);
	Dxl.moveServo(1, pick_start_1, false, true, false);

	// Last, lower pick to start position, either immediately or carefully (checking for contact)
	
	if (!careful) {
		//boost_sleep_ms(400);
		Dxl.moveServo(1, pick_start_1 + servo1_offset, false, false, false);
	}

	else {
		boost_sleep_ms(100);
		int N_steps = pick_start_1 + servo1_offset - getServoPos(1);
		// Move step by step instead of all at once
		for (int i = 0; i < N_steps/2; i++) {
			Dxl.moveServo(1, 2, true, true);	/// Move down 1 step
			boost_sleep_ms(10);					/// Wait a moment
			// Premature contact: Increase height, move, and report failure so we can try again
			if (!continue_lowering) {
				// focus_height = getPickupHeight() - 3;
				//Grbl2.goToAbsolute(Grbl2.getX(), Grbl2.getY(), focus_height, true, 20);
				Actx.moveAbsolute(focus_height);
				Err.notify(PRINT_ONLY, "E0: Pick touched surface while trying to lower to starting point");
				return false;
			}
		}
	}
	
	//boost_sleep_ms(400);

	/// Report pick position
	string msg = "\tPick (pickz,d1,d2,d3) - (" + to_string(Actx.readPosition()) + ","
		+ to_string(getServoPos(1)) + ","
		+ to_string(getServoPos(2)) + ","
		+ to_string(getServoPos(3)) + ")";
	Err.notify(PRINT_ONLY, msg);

	return true;
}


// Move the pick out of the way
bool WormPick::movePickAway() {

	if (!resetServoToMin(1)) return false;
	boost_sleep_ms(200);
	resetServo(2);
	boost_sleep_ms(200);
	resetServoToMin(3);	
	return true;
}

// Move the pick slightly out of the way
bool WormPick::movePickSlightlyAway() {

	resetServoToMin(1);
	boost_sleep_ms(100);
	moveServo(3, -180, true, true);
	boost_sleep_ms(100);
	return true; 
}

bool WormPick::movePickSlightlyAbovePlateHeight(bool move_pick_out_of_view) {
	moveServo(1, servo1_plate_height, false, true);
	//boost_sleep_ms(100);

	if (move_pick_out_of_view) {
		moveServo(2, pick_center_2, false, true);
		moveServo(3, pick_center_3 -150, false, true);
		//boost_sleep_ms(100);
	}
	return true;
}

/// Raise pick just a little bit up from the agar (for VquickPick)
/// stepsz1-3: step size of each motor, ratio between them determines the angle, magnitude determines the speed
bool WormPick::VmovePickSlightlyAway(int stepsz1, int stepsz2, int stepsz3, bool move_above_edge) {
	for (int i = 0; i < 5; i++) {
		Dxl.moveServo(1, stepsz1, true, true);
		Dxl.moveServo(3, stepsz3, true, true);
	}

	if (move_above_edge)
		movePickSlightlyAbovePlateHeight();
	else
		movePickSlightlyAway();
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
double WormPick::getPickupHeight() { return focus_height; }
int WormPick::getGrblCenterY() { return grbl_center_y; }
int WormPick::getGrblCenterZ() { return grbl_center_z; }
bool WormPick::getFinishedCenter() { return finished_centering; }
int WormPick::getIntensity() { return (int) intensity;  }
cv::Point WormPick::getPickTip() const { return pick_tip; }
double WormPick::getPickGradient(bool filtered_flag) const { 
	if (filtered_flag)
		return pick_gradient;
	else
		return pick_local_intensity;
}
double WormPick::getLocalMaxChange() const { return pick_local_max_change; }


cv::Rect WormPick::getCaliRegion() const {
	return Rect(Point(min_x, min_y), Point(max_x, max_y));
}


cv::Point WormPick::getTargetPt() const {
	return target_pt;
}

// Setter
void WormPick::resetIntensity() { intensity = 0;  }

void WormPick::setPickCenter3() { pick_center_3 = Dxl.positionOfServo(3); }
void WormPick::setPickCenter3(int setpos) { pick_center_3 = setpos; }
void WormPick::setPickCenter(int s1, int s2, int s3, double gy, double gz, double fh) {
	pick_start_1 = s1;
	pick_center_2 = s2;
	pick_center_3 = s3;

	grbl_center_y = gy;
	grbl_center_z = gz;

	focus_height = fh;

	savePickupInfo();
}
void WormPick::setPickStartServo1(int setpos) { pick_start_1 = setpos; }
void WormPick::setPickupAngle(int setpos) { pickup_angle = setpos; }
void WormPick::setcalibr_servo_coords(int start_pos1, int start_pos2, int start_pos3, int range, int step_sz) {

	// Start_pos1,2,3: The start position of servo motor in calibration, corresponding to the right upper corner of the calibration FOV
	// range: the movement range of servo motors in calibration
	// step_sz: the step size of servo motors in calibration

	calibr_servo_coords.clear();
	int step_num = round(range / step_sz);

	for (int i = 0; i < step_num; i++) {
		for (int j = 0; j < step_num; j++) {
			calibr_servo_coords.push_back(Point3i(start_pos1 - range/2 + i * step_sz, start_pos2 - range / 2 + j * step_sz, start_pos3));
		}
	}

}


void WormPick::setcalibr_tip_coords(const vector<cv::Point>& vec_pts) {
	calibr_tip_coords.clear();

	for (int i = 0; i < vec_pts.size(); i++) {
		calibr_tip_coords.push_back(vec_pts[i]);
	}
}


void WormPick::settip_servo_map(const std::vector<cv::Point>& tip_coords, const std::vector<cv::Point>& tip_coords_conv, const std::vector<cv::Point3i>& servo_coords, bool write_flag) {
	
	// tip_coords: coordinates computed by CNN
	// tip_coords_conv: dummy input, coordinates computed by conventional methods
	
	tip_servo_map.clear();

	if (tip_coords.size() == servo_coords.size()) {
		for (int i = 0; i < tip_coords.size(); i++) {
			tip_servo_map.push_back(std::pair<cv::Point, cv::Point3i>(tip_coords[i], servo_coords[i]));
		}
	}

	// Update the boundary of calibration region
	updateCaliMapBound();

	// Write the pick tip positions computed by CNN to file
	if (write_flag) {

		cout << "Write the pick calibration data to file..." << endl;

		ofstream pickdata;
		pickdata.open("C:\\WormPicker\\PickCalibration\\PickMap.txt", std::fstream::trunc);

		if (pickdata.is_open()) {
			for (int j = 0; j < tip_servo_map.size(); j++) {
				pickdata << tip_servo_map[j].first.x << "\t" << tip_servo_map[j].first.y << "\t" <<
					tip_servo_map[j].second.x << "\t" << tip_servo_map[j].second.y << "\t" << tip_servo_map[j].second.z;
				pickdata << endl;
			}
			pickdata.close();

		}
		else {
			cout << "Cannot open pick calibration file (CNN)!" << endl;
		}

		// Write the pick tip positions computed by conventional methods to file
		// Dummy write to file (this file will be no longer further used)
		ofstream pickdata_conv;
		pickdata_conv.open("C:\\WormPicker\\PickCalibration\\PickMap_conv.txt", std::fstream::trunc);

		if (pickdata_conv.is_open()) {
			for (int j = 0; j < tip_coords_conv.size(); j++) {
				pickdata_conv << tip_coords_conv[j].x << "\t" << tip_coords_conv[j].y << "\t" <<
					tip_servo_map[j].second.x << "\t" << tip_servo_map[j].second.y << "\t" << tip_servo_map[j].second.z;
				pickdata_conv << endl;
			}
			pickdata_conv.close();

		}
		else {
			cout << "Cannot open pick calibration file (conv)!" << endl;
		}
	}
}

void WormPick::stopLowering() { 
	continue_lowering = false; 
	abort_lowering = true;
}
