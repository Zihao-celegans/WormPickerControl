#include "WormShape.h"
#include "ImageFuncs.h"
#include "AnthonysCalculations.h"
#include "showDebugImage.h"
#include "DnnHelper.h"

using namespace std;
using namespace cv;

// Static variable definitions
std::vector<double>  WormShape::intensity_template = {};
dnn::Net WormShape::picker_net_hi_mag = dnn::readNetFromONNX(filenames::cnn_picker_hi_mag);

// Constructors
//*********************************************** Not used
WormShape::WormShape() {
	contour.clear();
	area = boost::none;
	centroid.x = -1;
	centroid.y = -1;
	roi.x = -1;
	roi.y = -1;
	trackTimer.setToZero();
	distanceTraveled = 0;
	sex = 0.5; //% likelihood of being male
	min_contour_size = 10; // minimum length of the contour to proceed with segmentation
	contour_jump = 5; // How much to jump (or smooth) as getting curvature of the contour. Must be much smaller than min_contour_size.
	loadIntensityTemplate();
	assert(contour_jump <= min_contour_size, "contour_jump must be <= to min_contour_size");
}

WormShape::WormShape(const vector<Point> &_contour){
	contour = _contour;
	area = boost::none;
	initial_area = getArea();
	initial_position = getWormCentroid();
	centroid = getCentroid(contour);
	initial_sex = sex;
	distanceTraveled = 0;
	min_contour_size = 10;
	loadIntensityTemplate();
	return;
}

WormShape::~WormShape() {
	return;
}

// Load the worm intensity template if it hasn't already been loaded by a previous WormShape
void WormShape::loadIntensityTemplate() {

	// Exit if already loaded
	if (intensity_template.size() > 0)
		return;

	// Load file
	ifstream infile(filenames::worm_int_profile);
	if (!infile.is_open()) throw std::runtime_error("Could not open CSV file: " + filenames::worm_int_profile);

	double val = 1;
	
	while (infile >> val) {
		intensity_template.push_back(val);
	}

	// Ensure that it's 100 elements long
	if (!intensity_template.size() == 100) throw std::runtime_error("Intensity profile loaded from CSV file: " + filenames::worm_int_profile + " is not 100 elements long.");

	// Normalize it to 0~255
	//vectorNorm(intensity_template, 255.0);
}


/*
	Worm geometry calculations
*/

std::vector<double> WormShape::getAngles(const std::vector<cv::Point>& wormBry) {
	// Appending Angles between groups of points (I'm sure there's a prettier way of doing this)
	double angle, absangle;
	vector<double> angles;
	vector<double> absangles;

	///Taking into consideration all cases for smoothening
	for (int i = 0; i < wormBry.size(); i++) {
		int j = contour_jump;
		///Making sure that we consider all points behind and in front of current point when smoothening
		if (i + j >= wormBry.size()) {
			angles.push_back(ptAngle(wormBry[i - j], wormBry[i], wormBry[(i + j) - wormBry.size()]));
			absangles.push_back(abs(ptAngle(wormBry[i - j], wormBry[i], wormBry[(i + j) - wormBry.size()])));
		}
		else if (i - j < 0) {
			// cout << i << " " << j << endl;
			angles.push_back(ptAngle(wormBry[wormBry.size() - (j - i)], wormBry[i], wormBry[(i + j)]));
			absangles.push_back(abs(ptAngle(wormBry[wormBry.size() - (j - i)], wormBry[i], wormBry[(i + j)])));
		}
		else {
			angles.push_back(ptAngle(wormBry[i - j], wormBry[i], wormBry[i + j]));
			absangles.push_back(abs(ptAngle(wormBry[i - j], wormBry[i], wormBry[i + j])));
		}
	}
	return absangles;
}

// Get the location of head and tail (not disambiguated, head assigned lower index)
void WormShape::getCandidateHeadTail(const std::vector<cv::Point>& wormBdry, int& loc1, int& loc2, double min_rel_dist) {

	// min_rel_dist: the minimum distance (fraction of the vector length) between two sharpest angles
	// Therefore, cannot be larger than 0.5
	// Default value is 0.3

	vector<double> contourAngles = getAngles(wormBdry);
	loc1 = 0; loc2 = 0;
	// Get the location that has the sharp angle
	for (int i = 0; i < contourAngles.size(); i++) {
		if (contourAngles[i] < contourAngles[loc1]) {
			loc1 = i;
		}
	}

	// Get the other sharp angle
	int a, b, c, d = -1; // define the interval of searching region
	
	if (loc1 - min_rel_dist * wormBdry.size() >= 0 && loc1 + min_rel_dist * wormBdry.size() <= wormBdry.size()) {

		a = 0;
		b = loc1 - min_rel_dist * wormBdry.size();
		c = loc1 + min_rel_dist * wormBdry.size();
		d = wormBdry.size() - 1;

		for (int j = a; j <= b; j++) {
			if (contourAngles[j] < contourAngles[loc2]) {
				loc2 = j;
			}
		}

		for (int k = c; k <= d; k++) {
			if (contourAngles[k] < contourAngles[loc2]) {
				loc2 = k;
			}
		}
	}
	else {
		if (loc1 - min_rel_dist * wormBdry.size() < 0) {
			a = loc1 + min_rel_dist * wormBdry.size();
			b = wormBdry.size() + loc1 - min_rel_dist * wormBdry.size();
		}
		else if (loc1 + min_rel_dist * wormBdry.size() > wormBdry.size()) {
			a = loc1 + min_rel_dist * wormBdry.size() - wormBdry.size();
			b = loc1 - min_rel_dist * wormBdry.size();
		}

		loc2 = a;
		for (int m = a; m <= b; m++) {
			if (contourAngles[m] < contourAngles[loc2]) {
				loc2 = m;
			}
		}
	}

	// Assign the head to the lower index
	if (loc2 < loc1) {
		int temp = loc2;
		loc2 = loc1;
		loc1 = temp;
	}

}

//**************************************************************** Not used, only the non-overloaded version is used.
// Overloaded function of getHeadTail: take two pairs of location and angle of candidate head and tail as input
void WormShape::getCandidateHeadTail(const std::vector<cv::Point>& wormBdry, std::pair<int, double>& loc_ang_pr1, std::pair<int, double>& loc_ang_pr2, double mini_dist) {

	vector<double> contourAngles = getAngles(wormBdry);

	int loc1, loc2 = 0;

	getCandidateHeadTail(wormBdry, loc1, loc2, mini_dist);

	loc_ang_pr1.first = loc1; loc_ang_pr1.second = contourAngles[loc1];
	loc_ang_pr2.first = loc2; loc_ang_pr2.second = contourAngles[loc2];
}


void WormShape::flipWormGeometry() {

	// If current orientation is not correct, flip the entire anatomy
	int temp = idx_tail;
	idx_tail = idx_head;
	idx_head = temp;

	pt_head = contour[idx_head];
	pt_tail = contour[idx_tail];

	reverse(dorsal_contour.begin(), dorsal_contour.end());
	reverse(ventral_contour.begin(), ventral_contour.end());
	reverse(bf_profile.begin(), bf_profile.end());

}


bool WormShape::segmentWorm(cv::Mat img, int bin_num, double width) {

	/*  Clear any existing "current" geometry */
	contour.clear();
	ventral_contour.clear();
	dorsal_contour.clear();
	bf_profile.clear();
	bf_profile_history.clear();
	fluo_profile.clear();
	center_line.clear();
	regional_masks.clear();
	head_history.clear();

	/* STEP 0: Constrain worm location using CNN to find it */
	double dsfactor = 0.25;
	Mat bw_cnn;
	int display_class_idx = 0;
	double conf_thresh = 4.0;
	int norm_type = 1;
	int out_img_mode = 2; // Binary
	Mat img_copy;
	img.copyTo(img_copy); // Deep copy the image as it will be modified by findWormsInImage

	CnnOutput out = findWormsInImage(img_copy, picker_net_hi_mag, dsfactor, bw_cnn, false, out_img_mode,
										display_class_idx, conf_thresh, norm_type);
	resize(bw_cnn, bw_cnn, img.size());  ///Resize back to non-downsampled size
	dilate(bw_cnn, bw_cnn, Mat::ones(Size(13, 13), CV_8U)); /// Dilate CNN mask to avoid clipping worm
	cnn_contour = isolateLargestContour(bw_cnn, min_contour_size);
	

	/* STEP 1: Approximate location of worm */

	// Blur the image
	Mat img_filt;
	int filt_size = 19;
	GaussianBlur(img, img_filt, Size(filt_size, filt_size), filt_size, filt_size);

	// Threshold the image and get largest contour
	Mat bw;
	threshold(img_filt, bw, 130, 255, THRESH_BINARY_INV); /// Threshold
	bw = bw.mul(bw_cnn);										/// Limit to worm-activated areas determined by CNN
	contour = isolateLargestContour(bw, min_contour_size);

	// Return false if no large contour found
	if (contour.size() < min_contour_size)
		return false;
		
	/* STEP 2: refine boundaries of worm using flowPass */
	// contour = flowPass(img, contour);  Doesn't make much better than just thresholding


	/* STEP 3: Get head/tail/geometry */

	// Get the candidate head and tail locations (NOT yet disambiguated, head assigned lower idx)
	getCandidateHeadTail(contour, idx_head, idx_tail);

	// Get the dorsal and ventral boundary
	splitContour(contour, idx_head, idx_tail, dorsal_contour, ventral_contour);

	// Resample the dorsal and ventral boundaries into multiple bins along the body
	ResampleCurve(dorsal_contour, dorsal_contour, bin_num);
	ResampleCurve(ventral_contour, ventral_contour, bin_num);

	if (dorsal_contour.size() <= 1 || ventral_contour.size() <= 1) {
		cout << "Fail to resample curve!" << endl;
		return false;
	}

	// Double check with assesshead and tail!!!!
	// Get the mask of each bin along the body
	getCoordMask(regional_masks, Size(img.cols, img.rows), dorsal_contour, ventral_contour, width);

	// Get the intensity plot along the body coordinate
	getBinMeanInten(bf_profile, img, regional_masks);

	// Ensure that the intensity template is resampled to the same length as the intensity curves
	intensity_template = vectorDownsample(intensity_template, bin_num);

	// Store location of head and tail
	pt_head = contour[idx_head];
	pt_tail = contour[idx_tail];

	// Disambiguate head and tail by comparing the intensity profile to a template
	bool correct_orientation = checkHeadTailCorr(intensity_template,bf_profile);


	// Copy the anatomy to output values
	if (!correct_orientation) 
		flipWormGeometry();
	


	// track the last 20 identified head locations, and swap head-tail if head not closest
	/// (Note the last-20 is purely as identified above, not including this continuity check
	head_history.push_back(pt_head);
	if (head_history.size() > 15) { head_history.pop_front(); }

	/// Compare historical position to current and flip if needed
	vector<Point> head_history_vect = { head_history.begin(),head_history.end() };
	avg_head_pos = contourMedian(head_history_vect);
	///if (ptDist(avg_head_pos, pt_head) > ptDist(avg_head_pos, pt_tail)) 
	///	flipWormGeometry();
	


	// get the centerline skeleton
	getCenterline(center_line, dorsal_contour, ventral_contour);

	return true;

}

void WormShape::fluoProfile(const Mat& fluo_img) {

	// Get the intensity plot along the body coordinate
	getBinMeanInten(fluo_profile, fluo_img, regional_masks);

}


// This function is NOT ventral/dorsal disambiguated. TO DO: disambiguate dorsal and ventral for other applications
void WormShape::splitContour(const std::vector<cv::Point>& wholeBry, int& loc1, int& loc2, std::vector<cv::Point>& DrlBdry, std::vector<cv::Point>& VtrlBdry) {

	// Copy the one portion to one boundary, in forward direction
	for (int i = loc1; i <= loc2; i++) {
		// cout << "i = " << i << endl;
		DrlBdry.push_back(wholeBry[i]);
	}

	// Copy the other portion, in reversed direction
	// (j%wholeBry.size()) != loc1
	for (int j = loc2, k = 0; k < (wholeBry.size() - DrlBdry.size() + 2); j++, k++) {
		// cout << "j = " << j << endl;
		// cout << "mode = " << j % wholeBry.size() << endl;
		VtrlBdry.push_back(wholeBry[j%wholeBry.size()]);
	}
	VtrlBdry.push_back(wholeBry[loc1]);

	// Flip the reversed portion
	reverse(VtrlBdry.begin(), VtrlBdry.end());
}


void WormShape::getCoordMask(std::vector<cv::Mat>& Masks, const cv::Size imgsz, const std::vector<cv::Point>& Bdry1, const std::vector<cv::Point>& Bdry2, double width) {
	
	
	for (int i = 0; i < (Bdry1.size() - 1); i++) {
		// Construct a polygon mask
		Mat mask;
		mask = Mat::zeros(imgsz, CV_8UC1);
		Point pts[1][4];


		//pts[0][0] = Bdry1[i];
		//pts[0][1] = Bdry2[i];
		//pts[0][2] = Bdry2[i+1];
		//pts[0][3] = Bdry1[i + 1];


		double p0x, p1x, p2x, p3x;
		double p0y, p1y, p2y, p3y;

		p0x = Bdry1[i].x + (Bdry2[i].x - Bdry1[i].x) * (1 - width) * 0.5;
		p0y = Bdry1[i].y + (Bdry2[i].y - Bdry1[i].y) * (1 - width) * 0.5;
		pts[0][0] = Point(p0x, p0y);

		p1x = Bdry1[i].x + (Bdry2[i].x - Bdry1[i].x) * (0.5 + 0.5 * width);
		p1y = Bdry1[i].y + (Bdry2[i].y - Bdry1[i].y) * (0.5 + 0.5 * width);
		pts[0][1] = Point(p1x, p1y);

		p2x = Bdry2[i + 1].x + (Bdry1[i + 1].x - Bdry2[i + 1].x) * (1 - width) * 0.5;
		p2y = Bdry2[i + 1].y + (Bdry1[i + 1].y - Bdry2[i + 1].y) * (1 - width) * 0.5;
		pts[0][2] = Point(p2x, p2y);

		p3x = Bdry2[i + 1].x + (Bdry1[i + 1].x - Bdry2[i + 1].x) * (0.5 + 0.5 * width);
		p3y = Bdry2[i + 1].y + (Bdry1[i + 1].y - Bdry2[i + 1].y) * (0.5 + 0.5 * width);
		pts[0][3] = Point(p3x, p3y);

		const Point* ppt[1] = { pts[0] };
		int npt[] = { 4 };
		fillPoly(mask, ppt, npt, 1, Scalar(255), LINE_8);
		
		Masks.push_back(mask);

	}

	// Fill end with the same mask as end-1 so it ends up at the requested N e.g. 100
	Masks.push_back(Masks[Masks.size()-1]);

}

//**************************************************************************** Not used
// get the region mask along the body coordinate. Ex. to get 0 - 10% region, set start_loc = 0, end_loc = 0.1
void WormShape::getRegionMask(cv::Mat& Mask, cv::Size imgsz, const std::vector<cv::Point>& Bdry1, const std::vector<cv::Point>& Bdry2, double start_loc, double end_loc, double width) {

	// Start & end location
	int start = round(Bdry1.size()*start_loc);
	int end = round(Bdry1.size()*end_loc);

	Mat RegionMask = Mat::zeros(imgsz, CV_8UC1);
	for (int i = start; i < end - 1; i++) {
		// Construct a polygon mask
		Mat mask; mask = Mat::zeros(imgsz, CV_8UC1);
		Point pts[1][4];


		// Compute the coordinates of vertex
		//pts[0][0] = Bdry1[i]; 
		//pts[0][1] = Bdry2[i]; 
		//pts[0][2] = Bdry2[i + 1]; 
		//pts[0][3] = Bdry1[i + 1];

		double p0x, p1x, p2x, p3x;
		double p0y, p1y, p2y, p3y;

		p0x = Bdry1[i].x + (Bdry2[i].x - Bdry1[i].x) * (1 - width) * 0.5;
		p0y = Bdry1[i].y + (Bdry2[i].y - Bdry1[i].y) * (1 - width) * 0.5;
		pts[0][0] = Point(p0x, p0y);

		p1x = Bdry1[i].x + (Bdry2[i].x - Bdry1[i].x) * (0.5 + 0.5 * width);
		p1y = Bdry1[i].y + (Bdry2[i].y - Bdry1[i].y) * (0.5 + 0.5 * width);
		pts[0][1] = Point(p1x, p1y);

		p2x = Bdry2[i + 1].x + (Bdry1[i + 1].x - Bdry2[i + 1].x) * (1 - width) * 0.5;
		p2y = Bdry2[i + 1].y + (Bdry1[i + 1].y - Bdry2[i + 1].y) * (1 - width) * 0.5;
		pts[0][2] = Point(p2x, p2y);

		p3x = Bdry2[i + 1].x + (Bdry1[i + 1].x - Bdry2[i + 1].x) * (0.5 + 0.5 * width);
		p3y = Bdry2[i + 1].y + (Bdry1[i + 1].y - Bdry2[i + 1].y) * (0.5 + 0.5 * width);
		pts[0][3] = Point(p3x, p3y);


		const Point* ppt[1] = { pts[0] };
		int npt[] = { 4 };
		fillPoly(mask, ppt, npt, 1, Scalar(255), LINE_8);
		/*namedWindow("Mask", WINDOW_AUTOSIZE);
		imshow("Mask", mask);
		waitKey(500);*/
		bitwise_or(mask, RegionMask, RegionMask);
	}

	imshow("Mask", RegionMask);
	waitKey(1);

	RegionMask.copyTo(Mask);
}

void WormShape::getBinMeanInten(std::vector<double>& outvec, const cv::Mat& inputary, const std::vector<cv::Mat>& Masks) {

	//cout << "Mean Intensity in each bin:" << endl;

	for (int i = 0; i < Masks.size(); i++) {
		cv::Scalar scalar = cv::mean(inputary, Masks[i]);
		//cout << scalar[0] << endl;
		outvec.push_back(scalar[0]);
	}
}

// Get the center line of worm skeleton
void WormShape::getCenterline(std::vector<cv::Point>& ctrline, const std::vector<cv::Point>& Bdry1, const std::vector<cv::Point>& Bdry2) {
	for (int i = 0; i < Bdry1.size(); i++) {
		ctrline.push_back(Point((Bdry1[i].x + Bdry2[i].x)*0.5, (Bdry1[i].y + Bdry2[i].y)*0.5));
	}
}

//**************************************************************************** Not used
void WormShape::getSkeletonMovement(const std::vector<cv::Point>& ctrline1, const std::vector<cv::Point>& ctrline2, double& movement, double start_loc, double end_loc, std::vector<cv::Point>& moving_p1, std::vector<cv::Point>& moving_p2) {

	movement = 0;
	double movement1 = 0, movement2 = 0;
	for (int i = 0; i < ctrline1.size(); i++) {
		movement1 = movement1 + ptDist(ctrline1[i], ctrline2[i]);
	}

	std::vector<cv::Point> inver_ctrline2(ctrline2);
	reverse(inver_ctrline2.begin(), inver_ctrline2.end());

	for (int i = 0; i < ctrline1.size(); i++) {
		movement2 = movement2 + ptDist(ctrline1[i], inver_ctrline2[i]);
	}


	int start = round(ctrline1.size()*start_loc);
	int end = round(ctrline1.size()*end_loc);
	// Make sure the orientation of ctrline1 & 2 are cooresponding to each other
	// The correct oritentation should yield smaller total movement
	if (movement1 > movement2) {
		for (int i = start; i < end; i++) {
			movement = movement + ptDist(ctrline1[i], inver_ctrline2[i]);

			moving_p1.push_back(ctrline1[i]);
			moving_p2.push_back(inver_ctrline2[i]);

		}
	}
	else {
		for (int i = start; i < end; i++) {
			movement = movement + ptDist(ctrline1[i], ctrline2[i]);

			moving_p1.push_back(ctrline1[i]);
			moving_p2.push_back(ctrline2[i]);
		}
	}
}

//*************************************************************** Not used, Better version of method is below
// Disambiguating head and tail of a worm by comparing intensity profile to template
bool WormShape::checkHeadTail(std::vector<double> body_intensity) {

	// Normalize the body intensity (copy) and create a flipped copy
	//vectorNorm(body_intensity,255.0);
	vector<double> flipped_body_intensity(body_intensity);
	reverse(flipped_body_intensity.begin(), flipped_body_intensity.end());

	// Validate that vectors are same size
	if (body_intensity.size() != intensity_template.size()) 
		throw std::runtime_error("Vector sizes did not match");

	// Compute Absolute mean difference between original and flipped intensity profiles and template
	double abs_diff_orig = 0, abs_diff_flipped = 0;
	for (int i = 0; i < body_intensity.size(); i++) {
		abs_diff_orig += abs(body_intensity[i] - intensity_template[i]);
		abs_diff_flipped += abs(flipped_body_intensity[i] - intensity_template[i]);
	}

	// Return true if original diff is less than flipped diff
	if (abs_diff_orig < abs_diff_flipped) {
		return true;
	}
	else {
		return false;
	}
}

// Improved version of head and tail disambiguation
// Based on an empirical rule: image intensity shows different patterns along the body
// Use scattering pattern matching (correlation coefficient) to measure whether current orientation is correct.
// Details see weekly updates on 2020 Oct 5
bool WormShape::checkHeadTailCorr(const std::vector<double>& templates, const std::vector<double>& body_inten) {

	// templates: vectors containing two intensity templates. templates[0]: hermaphrodite templates; templates[1]: male templates;
	// body_inten: vector whose orientation to be determined
	// return true if the vector has correct orientation, i.e. front of vector is head. Otherwise, return false.

	// Calculate the correlation coefficient between body intensity and templates, and calculate the sum
	double corr = corrcoef(templates, body_inten);

	// Flip the orientation of body intensity, calculate the correlation coefficient with templates
	vector<double> flip_body_inten(body_inten);
	reverse(flip_body_inten.begin(), flip_body_inten.end());
	double corr_flip = corrcoef(templates, flip_body_inten);


	// The correct orientation is supposed to have higher correlation
	if (corr >= corr_flip) {
		return true;
	}
	else {
		return false;
	}
}


// ************************************************************************ Only used in TestScripts.... but this feels useful and like something we should keep?
/// Draw the current WormShape's segmentation on an image. Adapted from the former Worms -> PhenotypeFluo
void WormShape::drawSegmentation(cv::Mat& img_color, bool debug) {

	// Ensure that img is a CV_8UC3 (color 8 bit image), unless we're going to draw a new debug image
	if (img_color.type() != CV_8UC3) {
		cout << "ERROR: drawSegmentation aborted because image was type " << img_color.type() << " instead of type 16  8UC3";
		return;
	}

	// Draw head and tail
	circle(img_color, pt_tail, 10, colors::rr, 2);
	drawCross(img_color, pt_head, colors::gg, 2, 15);
	circle(img_color, avg_head_pos, 10, colors::gg, -1);


	// Draw dorsal ventral boundary, both same color for now bc not disambuguated
	polylines(img_color, dorsal_contour, false, colors::gg, 2);
	polylines(img_color, ventral_contour, false, colors::gg, 2);

	// Draw CNN activated outline
	polylines(img_color, cnn_contour, true, colors::gg, 1);

	// Draw centerline
	polylines(img_color, center_line, false, colors::mm, 1);
	//circle(img_color, center_line[0], 5, colors::mm, -1);

	// Show debug image if using debug mode

	if (debug) {
		namedWindow("drawSegmentation debug", WINDOW_AUTOSIZE);
		imshow("drawSegmentation debug", img_color);
		waitKey(1);
	}


}



//*************************************************************************** This is used in multiple places (segmentWorm, in Worms.cpp also) but each place it is commented out
std::vector<cv::Point> WormShape::flowPass(const cv::Mat &frame, std::vector<cv::Point> last_contour) {


	if (last_contour.empty()) {
		throw EmptyWormException();
	}

	int dilationFactor = 6;

	//crop the frame to region around the tracked contour
	Mat crop = frame.clone();
	Rect crop_region = boundingRect(last_contour);
	crop_region += Size(30, 30);
	crop_region.x -= 15;
	crop_region.y -= 15;

	//edit region to prevent OOB crop
	crop_region.x = max(0, crop_region.x);
	crop_region.y = max(0, crop_region.y);
	crop_region.width = min(crop_region.width, crop.cols - crop_region.x);
	crop_region.height = min(crop_region.height, crop.rows - crop_region.y);

	crop = Mat(crop, crop_region); // crop frame to region of interest

	Mat contourImg = Mat::zeros(crop_region.height, crop_region.width, 0);

	//offset the contour so its in the cropped frame
	for (int i = 0; i < last_contour.size(); i++) {
		last_contour[i].x -= crop_region.x;
		last_contour[i].y -= crop_region.y;
	}

	//draw the filled contour to an image
	vector<vector<cv::Point>> temp;
	temp.push_back(last_contour);
	fillPoly(contourImg, temp, 255);
	temp.~vector();

	//dialte the image
	dilate(contourImg, contourImg, getStructuringElement(MORPH_RECT, Size(dilationFactor, dilationFactor)));

	vector<int> intensities;

	//sort all pixels in the contour by order of intensity
	//will be used to calculate the threshold intensity later
	for (int i = 0; i < crop.rows; i++) {
		for (int j = 0; j < crop.cols; j++) {
			if (contourImg.at<uchar>(i, j) != 0) {
				intensities.push_back(crop.at<uchar>(i, j));
			}
		}
	}

	//sort the intensities of the points to find the top intensities
	sort(intensities.begin(), intensities.end(), less<int>());

	int flow_area = (int) contourArea(last_contour);

	if (intensities.size() < flow_area) {//the worm is lost
		throw WormLostException();
	}

	//threshold is the area^th darkest pixel in the contour
	if (intensities.size() < flow_area + 1) {//the worm is lost due to too small
		throw WormLostException();
	}
	int thresh = intensities[flow_area];

	float dark_avg = 0; //average intensity of all pixels in the contour
	for (int i = 0; i < flow_area; i++) {
		dark_avg += intensities[i];
	}
	dark_avg = dark_avg / flow_area;

	float light_avg = 0; //average intensity of all pixels that aren't in contour
	for (int i = flow_area; i < intensities.size(); i++) {
		light_avg += intensities[i];
	}
	light_avg = light_avg / (intensities.size() - flow_area);


	if (light_avg < (dark_avg * 1.7)) { // there is not a sufficient diff between light and dark pixels
		// the worm likely entered a dark clump of something
		//throw WormLostException();
	}

	//find all pixels in the crop that are in the dilated contour and match the thresh
	Mat bwout = Mat::zeros(crop.size(), 0);

	for (int i = 0; i < bwout.rows; i++) {
		for (int j = 0; j < bwout.cols; j++) {
			if (contourImg.at<uchar>(i, j) && (crop.at<uchar>(i, j) < thresh)) {
				bwout.at<uchar>(i, j) = 255;
			}
		}
	}

	//segment the pixels that meet the conditions
	vector<vector<cv::Point>> contoursTemp;
	findContours(bwout, contoursTemp, RETR_CCOMP, CHAIN_APPROX_SIMPLE);

	// Return if no contours to segment - otherwise below crashes
	if (contoursTemp.size() == 0) {
		throw WormLostException();
	}

	//find the index largest contour found
	int max_idx = 0;
	for (int i = 0; i < contoursTemp.size(); i++) {
		if (contoursTemp[i].size() > contoursTemp[max_idx].size()) {
			max_idx = i;
		}
	}

	vector<cv::Point> result;

	result = contoursTemp[max_idx];

	//offset the contour back onto the frame
	for (int i = 0; i < result.size(); i++) {
		result[i].x += crop_region.x;
		result[i].y += crop_region.y;
	}

	return result;
}







//Setter Functions

void WormShape::updateContour(const vector<Point> &_contour) {
	contour = _contour;
	area = boost::none;
	distanceTraveled += ptDist(centroid, getCentroid(contour));
	centroid = getCentroid(contour);
	return;
}

void WormShape::updateContourV2(const std::vector<cv::Point> &_contour) {
	contour = _contour;
	return;
}

//*********************************************************************** Not used
void WormShape::updateSex(int step){
	//sex =  calculateSexLikelihood(step);
}

void WormShape::reset(const vector<Point> &_contour) {
	updateContour(_contour);
	initial_position = getWormCentroid();
	initial_area = getArea();
	distanceTraveled = 0;
	trackTimer.setToZero();
}

void WormShape::reset_centroid(cv::Point& centroid_assign) {
	centroid = centroid_assign;
}

void WormShape::reset_roi(cv::Rect& roi_assign) {
	roi = roi_assign;
}


void WormShape::reset() {
	reset(contour);
}

void WormShape::clear() {
	contour.clear();
	area = -1;
	initial_area = -1;
	centroid.x = -1;
	centroid.y = -1;
	roi.x = -1;
	roi.y = -1;
	sex = 0.5;
}

//Getter Functions

bool WormShape::isEmpty() const {
	return contour.empty();
}

bool WormShape::isEmpty_centroid() const {
	return (centroid == Point(-1, -1));
}

bool WormShape::isEmpty_roi() const {
	return (roi.x == -1 && roi.y == -1);
}

float WormShape::getDistanceTraveled() const {
	return distanceTraveled;
}

//*************************************************************** Not used
float WormShape::getSex() const {
	return sex;
}

//*************************************************************** Not used
double WormShape::getTimeTracked() {
	return trackTimer.getElapsedTime();
}

cv::Point WormShape::getWormCentroid() const {
	return centroid;
}

cv::Rect WormShape::getWormRoi() const {
	return roi;
}

std::vector<cv::Point> WormShape::getContour() const {
	return contour;
}

float WormShape::getArea() {

	if (area) {
		return area.get();
	}
	else {
		if (isEmpty()) {
			area = 0;
			return 0;
		}
		area = contourArea(contour);
		return area.get();
	}
	
}


//********************************************************** This is only used in TestScript.cpp
float WormShape::getAreaV2() {

	if (!contour.empty()) {
		area = contourArea(contour);
		return area.get();
	}
	else {
		area = 0;
		return 0;
	}
}

Rect WormShape::boundingBox() const{
	return boundingRect(contour);
}

float WormShape::getInitialArea() const {
	return initial_area;
}

float WormShape::getDisplacement() const {
	return ptDist(initial_position, getWormCentroid());
}


//Misc Functions

//************************************************************************************* Not used
float WormShape::avgSurroundingIntensity(const Mat &img, float region_size) {
	Mat crop = img.clone();

	Rect crop_region = boundingRect(contour);

	//expand the region relative to the size of the frame
	crop_region.x = crop_region.x - (img.size().width * region_size) / 2;
	crop_region.y = crop_region.y - (img.size().height * region_size) / 2;
	crop_region.width = crop_region.width + (img.size().width * region_size) / 2;
	crop_region.height = crop_region.height + (img.size().height * region_size) / 2;

	//edit region to prevent OOB crop
	crop_region.x = max(0, crop_region.x);
	crop_region.y = max(0, crop_region.y);
	crop_region.width = min(crop_region.width, crop.cols - crop_region.x);
	crop_region.height = min(crop_region.height, crop.rows - crop_region.y);

	crop = Mat(crop, crop_region); // crop frame to region of interest

	vector<int> intensities;

	for (int i = 0; i < crop.cols; i++) {
		for (int j = 0; j < crop.rows; j++) {
			intensities.push_back(crop.at<uchar>(j, i));
		}
	}

	sort(intensities.begin(), intensities.end(), less<int>());

	float this_avg = 0;
	for (int i = getArea(); i < intensities.size(); i++) {
		this_avg += intensities[i];
	}

	return this_avg / (intensities.size() - getArea());
}

//********************************************************************** This is only used in places that have been commented out.
// Function that slices a vector, used for splitting the contour into top and bottom
template<typename T>
std::vector<T> slice(std::vector<T> const &v, int m, int n) {
	auto first = v.cbegin() + m;
	auto last = v.cbegin() + n + 1;
	std::vector<T> vec(first, last);
	return vec;
}
//
//// Function that calculates likelihood of a worm being a male
//double WormShape::calculateSexLikelihood(int step){
//	// In case contour can't be quantified
//	if (contour.empty()) {
//		return 0;
//	}
//
//	/* not necessary (i think)
//	//Removing any points that are far off based off the contour calculations:
//	int limit = 1000;
//	for (int i = 0; i < contour.size(); i++) {
//		if (abs(contour[i].x) > limit || abs(contour[i].y) > limit) {
//			contour.erase(contour.begin() + i);
//		}
//	} */
//	
//	// Angles vector for calculating head (used mostly for debugging)
//	vector<double> vecCurvature(contour.size());
//	vector<double> vecRawCurvature(contour.size());
//
//	// We're going to keep track of the smallest angles and their indices, these will be the tips of head and tail
//	int idx1 = 0;
//	int idx2 = 0;
//	double min1 = 360;
//	double min2 = 360;
//	int min_distance = 8; /// minimum distance between head and tail since small angles might exist consecutivily in tips
//	   
//	// Appending Angles between groups of points (I'm sure there's a prettier way of doing this)
//	double angle, absangle; 
//	int smoothen_factor = 2;
//
//	///Taking into consideration all cases for smoothening
//		for (int i = 0; i < contour.size(); i++) {
//		
//		///Making sure that we consider all points behind and in front of current point when smoothening
//		vector<double> angles;
//		vector<double> absangles;
//
//		for (int j = 1; j < smoothen_factor; j++) {
//			if (i + j > contour.size()) {
//				angles.push_back(ptAngle(contour[i - j], contour[i], contour[(i+j) - contour.size()]));
//				absangles.push_back(abs(ptAngle(contour[i - j], contour[i], contour[(i + j) - contour.size()])));
//			}
//			else if (i - j < 0) {
//				angles.push_back(ptAngle(contour[contour.size() - (j-i)], contour[i], contour[(i + j) - contour.size()]));
//				absangles.push_back(abs(ptAngle(contour[contour.size() - (j - i)], contour[i], contour[(i + j) - contour.size()])));
//			}
//			else {
//				angles.push_back(ptAngle(contour[i - j], contour[i], contour[i + j]));
//				absangles.push_back(abs(ptAngle(contour[i - j], contour[i], contour[i + j])));
//			}
//		}
//
//		// Appending to list of angles
//		angle = vectorMean(angles);
//		absangle = vectorMean(absangles);
//		vecCurvature[i] = absangle;
//		vecRawCurvature[i] = angle;
//		
//		// Keeping track of top 2 tightest angles and indices as these refer to tips
//		if (absangle < min1) {
//			min2 = min1;
//			idx2 = idx1;
//			min1 = absangle;
//			idx1 = i;
//		}
//		else if ( absangle < min2 || angle == min1) {
//			///confirming that points are far apart from each other 
//			if (abs(i - idx1) > min_distance || abs(i - idx2) > min_distance){ 
//				min2 = absangle;
//				idx2 = i;
//			}
//		}
//	}
//
//	// Splitting vector into two halves based on indices to calculate width of heads
//	///top half
//	vector<Point> topHalf;
//	if (idx2 > idx1) {
//		topHalf = slice(contour, idx1, idx2);
//	}
//	else {
//		topHalf = slice(contour, idx2, idx1);
//		int temp = idx1;
//		idx1 = idx2;
//		idx2 = temp;
//	}
//	
//	///bottom half
//	vector<Point> cut1, cut2, bottomHalf;
//	cut1 = slice(contour, idx2, contour.size()-1);
//	cut2 = slice(contour, 0, idx1);	
//	bottomHalf.insert(bottomHalf.end(), cut1.begin(), cut1.end());
//	bottomHalf.insert(bottomHalf.end(), cut2.begin(), cut2.end());
//	reverse(bottomHalf.begin(), bottomHalf.end());
//
//	// Calculating size of tail and head bulges, notice that tail and head are interchangeable
//	double sizeTail = 0;
//	double sizeHead = 0;
//	int headPoints = 6;
//	for (int i = 0; i < headPoints; i++) {
//		sizeHead += ptDist(topHalf[i], bottomHalf[i]);
//	}
//	
//	// For the tail this becomes a bit more tricky but we can reverse the vector and index accordingly 
//	reverse(bottomHalf.begin(), bottomHalf.end());
//	reverse(topHalf.begin(), topHalf.end());
//	
//	for (int i = 0; i < headPoints; i++) {
//		sizeTail += ptDist(topHalf[i], bottomHalf[i]);
//	}
//
//	// Determining which is bigger, head or tail?
//	int headBigger = 0;
//	if (sizeTail < sizeHead) { 
//		headBigger = 1; 
//	}
//
//	// If sizes significantly different, then worm is a male (i think)
//	double likelihood;
//	if (headBigger == 1) {
//		likelihood = (abs(sizeTail - sizeHead)) / sizeHead;
//	}
//	else {
//		likelihood = (abs(sizeTail - sizeHead)) / sizeTail;
//	}
//
//	// If likelihood is 0 or infinity, it means that the calculations are off
//	if (likelihood == numeric_limits<double>::infinity() || likelihood == 0.0) {
//		return 0.0;
//	}
//	else {
//		return likelihood;
//	}
//
//}

//********************************************************** Only used in thinning(), which is only used in getSkeleton(), which is not used anywhere
// Applies a thinning iteration to a binary image
void WormShape::thinningIteration(Mat img, int iter) {
	Mat marker = Mat::zeros(img.size(), CV_8UC1);


	for (int i = 1; i < img.rows - 1; i++)
	{
		for (int j = 1; j < img.cols - 1; j++)
		{
			uchar p2 = img.at<uchar>(i - 1, j);
			uchar p3 = img.at<uchar>(i - 1, j + 1);
			uchar p4 = img.at<uchar>(i, j + 1);
			uchar p5 = img.at<uchar>(i + 1, j + 1);
			uchar p6 = img.at<uchar>(i + 1, j);
			uchar p7 = img.at<uchar>(i + 1, j - 1);
			uchar p8 = img.at<uchar>(i, j - 1);
			uchar p9 = img.at<uchar>(i - 1, j - 1);

			int A = (p2 == 0 && p3 == 1) + (p3 == 0 && p4 == 1) +
				(p4 == 0 && p5 == 1) + (p5 == 0 && p6 == 1) +
				(p6 == 0 && p7 == 1) + (p7 == 0 && p8 == 1) +
				(p8 == 0 && p9 == 1) + (p9 == 0 && p2 == 1);
			int B = p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9;
			int m1 = iter == 0 ? (p2 * p4 * p6) : (p2 * p4 * p8);
			int m2 = iter == 0 ? (p4 * p6 * p8) : (p2 * p6 * p8);

			if (A == 1 && (B >= 2 && B <= 6) && m1 == 0 && m2 == 0)
				marker.at<uchar>(i, j) = 1;
		}
	}
	
	img &= ~marker;
}

//********************************************************** Only used in getSkeleton(), which is not used anywhere
// Apply the thinning procedure to a given image
void WormShape::thinning(InputArray input, OutputArray output) {
	Mat processed = input.getMat().clone();
	// Enforce the range of the input image to be in between 0 - 255
	processed /= 255;

	Mat prev = Mat::zeros(processed.size(), CV_8UC1);
	Mat diff;

	do {
		thinningIteration(processed, 0);
		thinningIteration(processed, 1);
		absdiff(processed, prev, diff);
		processed.copyTo(prev);
	} while (countNonZero(diff) > 0);

	processed *= 255;

	output.assign(processed);
}

//******************************************************************************** Not used
Mat WormShape::getSkeleton() {

	if (isEmpty()) {
		throw "contour is empty";
	}

	Rect r = boundingRect(contour);

	//draw the contour to a matrix
	Mat img(r.y + r.height, r.x + r.width, CV_8UC1);

	Mat roi = Mat(img, r);

	//set the matrix to 0
	img = 0;
	
	vector<vector<Point>> temp;
	temp.push_back(contour);
	fillPoly(img, temp, 1);

	roi *= 255;
	Mat out;
	out = 0;

	thinning(roi, out);

	
	return out;
}


