#include "Worms.h"
#include "ImageFuncs.h"
#include "Config.h"
#include "DnnHelper.h"
#include "ControlPanel.h"
#include <vector>
#include <iostream>
#include <algorithm>
#include <iomanip>
#include <cmath>
#include "showDebugImage.h"


using namespace cv;
using namespace std;
using json = nlohmann::json;

// Constructor with default values for all thresholds

Worms::Worms(Size sz, int& wat, std::string fonnx_worm) :
	watching_for_picked(wat)
{
	// Preallocate bw image as the same size as the camera image
	bwcontours = Mat::zeros(sz, CV_8U);

	/*
			Read in image data from disk. Format:
			thresh		contrast_max
			area_min	area_max
			length_min	length_max
			lwratio_min	lwratio_max
	*/

	loadParameters();

	// pickup_location = Point(-1, -1);
	pickup_location = Point(320, 242);
	// pickup_location = Point(287, 200);

	// Tracking / picking / other flags
	overlayflag = true;

	wormSelectMode = false;
	trackingEnabled = true;

	// Initialize the deque of 20 frame for later retrieval
	/*vector<Mat> Sequence(20, Mat::zeros(imggray.size(), CV_8UC1));
	ImageQue = Sequence;*/
	//ImageQue.assign(100, Mat::zeros(sz, CV_8UC1));

	// Initialize the centroid with highest energy
	max_energy_centroid = Point(-1, -1);

	Diff_img = Mat::zeros(sz, CV_8U);

	findWorminWholeImg = true;
	findWormNearPickupLoc = false;
	trackwormNearPcikupLoc = false;
	trackMultipleWorm = false;
	worm_finding_mode = 0;
	limit_max_dist = true;
	trackbyCtrofMass = true;

	max_move = 10;

	offset = Point2d(0, 0);
	last_coordinate = Point2d(0,0);

	// ROI of pick-up location
	Point pickup_pt = pickup_location;
	pickup_Roi.x = pickup_pt.x - pickup_radius;
	pickup_Roi.y = pickup_pt.y - pickup_radius;
	pickup_Roi.width = 2 * pickup_radius;
	pickup_Roi.height = 2 * pickup_radius;

	//// Prevent OOB
	//pickup_Roi.x = max(pickup_Roi.x, 0);
	//pickup_Roi.y = max(pickup_Roi.y, 0);
	//pickup_Roi.width = min(pickup_Roi.width, imggray.size().width - pickup_Roi.x);
	//pickup_Roi.height = min(pickup_Roi.height, imggray.size().height - pickup_Roi.y);

	search_roi.x = 0;
	search_roi.y = 0;
	search_roi.width = sz.width;
	search_roi.height = sz.height;

	// Set the ControlPanel object, used to find worms in low magnification image
	ControlPanel ConPan(sz, filenames::cnn_picker_low_mag);
	ctrlPanel = ConPan;
	// Set the search region for verifying the unloading success
	unload_search_Roi.x = -1;
	unload_search_Roi.y = -1;
	unload_search_Roi.width = 0;
	unload_search_Roi.height = 0;


}

// Destructor

Worms::~Worms()
{
}

// Load parameters from disk. If bad parameters are detected, throw error
void Worms::loadParameters() {

	Config::getInstance().loadFiles();
	json config = Config::getInstance().getConfigTree("selection criteria");
	
	thresh = config["thresh"].get<int>();
	contrast_max = config["contrast max"].get<int>();
	area_min = config["area"]["min"].get<int>();
	area_max = config["area"]["max"].get<int>();
	length_min = config["length"]["min"].get<int>();
	length_max = config["length"]["max"].get<int>();
	lwratio_min = config["l:w ratio"]["min"].get<int>();
	lwratio_max = config["l:w ratio"]["max"].get<int>();

	// Validate data
	bool test1 = area_max > area_min;
	bool test2 = length_max > length_min;
	bool test3 = lwratio_max > lwratio_min;
	BOOST_ASSERT_MSG(test1 && test2 && test3, "ERROR: Invalid params in SortedBlobs file");

}

void Worms::writeParameters() {
	if (!isWormTracked()) {
		return;
	}
	Config::getInstance().loadFiles();
	json config = Config::getInstance().getConfigTree("selection criteria");

	double a = tracked_worm.getArea();
	double p = arcLength(tracked_worm.getContour(), true);
	double l = p / 2.0;
	double lw = l / (a / l);

	config["area"]["min"] = a * 0.5;
	config["area"]["max"] = a * 1.5;
	config["length"]["min"] = l * 0.5;
	config["length"]["max"] = l * 1.5;
	config["l:w ratio"]["min"] = lw * 0.5;
	config["l:w ratio"]["max"] = lw * 1.5;

	Config::getInstance().replaceTree("selection criteria", config);
	Config::getInstance().writeConfig();
	loadParameters();
}

///******************************************************************** Used in WormPicker.cpp, but is commented out.
void Worms::evaluateHiMagBlobs() {

	boost_sleep_ms(3000);

	for (;;) {

		Mat thres_img;
		Mat thres_pre_frm;

		// Smoothen image
		Size blursize(15, 15);
		int sigma = 2;
		GaussianBlur(img_high_mag, thres_img, blursize, sigma, sigma);
		GaussianBlur(pre_frame_hi_mag, thres_pre_frm, blursize, sigma, sigma); // TO DO: get the preframe

		// Threshold
		threshold(thres_img, thres_img, 70, 255, THRESH_BINARY_INV);
		threshold(thres_pre_frm, thres_pre_frm, 70, 255, THRESH_BINARY_INV);

		imshow("Thresh", thres_img);
		waitKey(1);


		// Remove very small objects prior to analysis
		/*Size removesmallsize(15, 15);
		Mat kernel = getStructuringElement(MORPH_ELLIPSE, removesmallsize);
		morphologyEx(thres_img, thres_img, MORPH_OPEN, kernel);

		imshow("Remove small blobs", thres_img);
		waitKey(1);*/

		// Label connected components using contours, an array of points

		//isolateLargestBlob(thres_img, 10);
		//imshow("Find largest blob", thres_img);
		//waitKey(1);

		//isolateLargestBlob(thres_img, 10);
		//isolateLargestBlob(thres_pre_frm, 10);

		Size removesmallsize(15, 15);
		Mat kernel = getStructuringElement(MORPH_ELLIPSE, removesmallsize);
		morphologyEx(thres_img, thres_img, MORPH_OPEN, kernel);
		morphologyEx(thres_pre_frm, thres_pre_frm, MORPH_OPEN, kernel);

		imshow("Remove small blobs", thres_img);
		waitKey(1);


		Mat Diff;
		absdiff(thres_img, thres_pre_frm, Diff);
		isolateLargestBlob(Diff, 5);
		imshow("Diff", Diff);
		waitKey(1);

		findoverlapBlob(Diff, thres_img); // Find the blobs that has movement in the past few second

		imshow("Moving", thres_img);
		waitKey(1);


		vector<vector<Point>> contoursTemp;
		vector<Vec4i> hierarchy;
		findContours(thres_img, contoursTemp, hierarchy, RETR_CCOMP, CHAIN_APPROX_SIMPLE);


		// If the blob matches the thresholds, draw it on the binary image
		drawContours(thres_img, contoursTemp, -1, colors::gg, 1, 8);

		// Measure geometry of this blob

		for (int i = 0; i < contoursTemp.size(); i++) {
			double A = contourArea(contoursTemp[i]);		// Area
			double P = arcLength(contoursTemp[i], true);	// Perimeter
			double L = P / 2;								// Approximate length 
			double W = A / L;								// Approximate width
			double LW = L / W;								// Length-to-Width ratio

			cout << "Area = " << A << endl;
			cout << "Perimeter = " << P << endl;
			cout << "Approx. length = " << L << endl;
			cout << "Approx. width = " << W << endl;
			cout << "Len-to-Wid ratio = " << LW << endl;
			
		}

		////////////////////////////////////////////////////////////////////////

		//threshold(img_high_mag, thres_img, 50, 255, THRESH_BINARY_INV);


		// Check for exit
		boost_interrupt_point();

		// Don't run too fast
		boost_sleep_ms(25);

	}

}

//rough pass that replaces extract blobs when in worm select mode
//all viable contours are outlined
int Worms::roughPass() {
	int cnt = 0;

	bwcontours.setTo(0);
	all_worms.clear();

	//if the contour is trivially big, add it contours
	for (int i = 0; i < contoursAll.size(); i++) {
		if (contoursAll[i].size() > 18) {
			cnt++;
			all_worms.push_back(WormShape(contoursAll[i]));
		}
	}

	return cnt;
}


int Worms::extractBlobs() {
	if (wormSelectMode) {
		return roughPass();
	}
	else if (!trackingEnabled) {
		return 0;
	}

	// Reset the extracted contours
	bwcontours.setTo(0);
	all_worms.clear();

	// Extract contours matching the requested perimiter and area values
	double	A, P, L, W, LW;
	int		ct = 0;

	for (int i = 0; i < contoursAll.size(); i++) {

		// Measure geometry of this blob
		A = contourArea(contoursAll[i]);		// Area
		P = arcLength(contoursAll[i], true);	// Perimeter
		L = P / 2;								// Approximate length 
		W = A / L;								// Approximate width
		LW = L / W;								// Length-to-Width ratio

		// Determine whether to keep this blob based on its geometry
		bool iskeep = A > area_min && A < area_max && L > length_min && L < length_max && LW > lwratio_min && LW < lwratio_max;

		// if the worm is the tracked worm, ignore it
		if (iskeep && (ptDist(getCentroid(contoursAll[i]), tracked_worm.getWormCentroid()) < 20 && isWormTracked())) {
			iskeep = false;
		}

		if (iskeep) {
			// Increment the extracted blob counter
			ct++;

			// If the blob matches the thresholds, draw it on the binary image
			drawContours(bwcontours, contoursAll, i, colors::ww, -1, 8);

			// If the blob matches the thresholds, add it to the extracted contous list
			all_worms.push_back(WormShape(contoursAll[i]));
		}
	}
	
	// Return the number of blobs
	return ct;
}


void Worms::evaluateBlobs(const cv::Mat &frame) {

	// Set the mask for image to be contours
	Mat tempimg, bwcontoursinv;
	frame.copyTo(tempimg);

	// Calculate the mean pixel value within the contours
	double meanworm = mean(tempimg, bwcontours)[0];

	// Calculate the mean pixel value within the background
	bitwise_not(bwcontours, bwcontoursinv);
	double meanbg = mean(tempimg, bwcontoursinv)[0];

	// Fill in the evaluation vector
	evalmetrics.clear();
	evalmetrics.push_back(meanworm);			// evalmetrics[0]	=	meanworm
	evalmetrics.push_back(meanbg);				// evalmetrics[1]	=	meanbg
	evalmetrics.push_back(meanworm / meanbg);	// evalmetrics[2]	=	meanworm/meanbg

	// Print results	
	// printf("thresh=%d\tN_blobs=%d\tcontrast=%0.2f\n", thresh, N_blobs, evalmetrics[2]);
}

///***************************************************************************** Started in its own thread in WormPicker.cpp, but is commented out.
void Worms::TrackWormCnn(const cv::Mat& imggray, const cv::dnn::Net& net) {

	// TODO - Remove hardcodes 
	double dsfactor = 0.4;
	double conf_thresh = 6.7;
	double conf_thresh_subseq_track = 3.2;
	int norm_type = 1;
	int radius_subseq_track = 50; // radius of region for subsequent tracking after a worm has been targeted

	for (;;) {

		// Check the center of mask region has been initialized

		// If centroid of tracked worm is empty in the previous frame
		// find a worm nearest to the pick-up location 
		if (tracked_worm.isEmpty_centroid()) {

			// Make local copy of image for resizing
			Mat img;
			imggray.copyTo(img);

			// Dummy imgout, we don't need it
			Mat imgout;

			// Run CNN to get output
			
			CnnOutput out = findWormsInImage(img, net, dsfactor, imgout, false, 0, 0, conf_thresh, norm_type);

			// Get the centroids of bins that contain worms
			vector<Point> worm_coords;
			vector<double> prob_coords;
			
			for (int i = 0; i < size(out.classes); i++)
			{
				if (out.classes[i] == 0) {
					worm_coords.push_back(out.coords[i] / dsfactor);
					prob_coords.push_back(out.probs[i]);
				}
			}

			// Points closed to each other are removed, represented by a point that has the highest probability among them
			for (int i = 0; i < size(worm_coords); i++) {
				for (int j = i + 1; j < size(worm_coords); j++) {
					double this_dist = ptDist(worm_coords[i], worm_coords[j]);
					if (this_dist < 20) {
						if (prob_coords[i] > prob_coords[j]) {
							worm_coords[j] = Point(0, 0);
						}
						else {
							worm_coords[i] = Point(0, 0);
						}
					}
				}
			}
			// Remove the point that are (0, 0)
			worm_coords.erase(remove(worm_coords.begin(), worm_coords.end(), Point(0, 0)), worm_coords.end());

			// Assign the coords to worms
			assigncentroidAll(worm_coords);

			double min_dist = 999;
			int min_idx = -1;

			// Find all the centroids and compare to the pickup location
			for (int i = 0; i < centroidAll.size(); i++) {
				double this_dist = ptDist(getPickupLocation(), centroidAll[i]);
				if (this_dist < min_dist) {
					min_dist = this_dist;
					min_idx = i;
				}
			}
			// Set the centroid point of tracked worm
			if (min_dist < track_radius * 2) {
				tracked_worm.reset_centroid(centroidAll[min_idx]);
			}
		}

		// If centroid of the tracked worm has already in theb previous frame, i.e. tracked_worm.getWormCentroid() != Point(-1, -1)
		// find worm-like objects near the centroid of tracked worm, within the tracking region.
		else {

			// Make local copy of image for resizing
			Mat image, Roi_img;
			imggray.copyTo(image);

			// Set the ROI of tracking region
			Rect ROI;
			ROI.x = tracked_worm.getWormCentroid().x - radius_subseq_track;
			ROI.y = tracked_worm.getWormCentroid().y - radius_subseq_track;
			ROI.width = 2 * radius_subseq_track;
			ROI.height = 2 * radius_subseq_track;

			// Prevent ROI out of bound
			ROI.x = max(ROI.x, 0);
			ROI.y = max(ROI.y, 0);
			ROI.width = min(ROI.width, image.size().width - ROI.x);
			ROI.height = min(ROI.height, image.size().height - ROI.y);

			// Crop the image in ROI
			image(ROI).copyTo(Roi_img);

			// Dummy imgout, we don't need it
			Mat imgout;

			// Run CNN to get output
			
			CnnOutput out_roi = findWormsInImage(Roi_img, net, dsfactor, imgout, false, 0, 0, conf_thresh, norm_type);

			if (out_roi.max_prob > conf_thresh_subseq_track) {
				// Get the coordinate of worm with respect to the ROI image
				Point posit_worm = out_roi.max_prob_point / dsfactor;
				// Convert the coordinate of worm respect to the whole image
				posit_worm.x = ROI.x + posit_worm.x;
				posit_worm.y = ROI.y + posit_worm.y;
				centroidAll.clear();
				tracked_worm.reset_centroid(posit_worm);
				tracked_worm.reset_roi(ROI);
			}
			// If the highest probability is less than a threshold value, we think that the worm is lost
			// and reset the centroid of tracked worm and roi
			else {
				tracked_worm.clear();
			}
		}


		// Check for exit
		boost_interrupt_point();

		// Don't run too fast
		//boost_sleep_ms(100);

	}

}

///***************************************************************************** Started in its own thread in WormPicker.cpp, but is commented out.
void Worms::FindWormEnergy( const cv::dnn::Net& SingleFrameNet, const cv::dnn::Net& MultiFrameNet) {

	// Parameters for single-frame CNN
	double dsfactor = 0.4;
	double conf_thresh = 5.7;
	double thresh_find_near_pickuploc = 2;
	int norm_type = 1;

	// Parameters for multi-frame CNN

	// Parameters for energy functional
	//Energy = alpha * DistCentroidtoPickupLoc[i] + beta * pixel_change_rois[i] + gamma * ConfiCentroid_singlefrmCnn[i];
	// for finding mode:
	double alpha = 0; // Should be negative
	double beta = 1;
	double gamma = 1;
	double energy_thresh = 5000;

	// for monitor mode:
	double alpha_mon = -1; 
	double beta_mon = 0;
	double gamma_mon = 0;

	// Parameters for monitor worms in ROIs
	int radius_Roi = 40; // ROI is analysized 3x3 kernel; if 50, analysized 4x4 kernel
	int radius_roi_small = 40;
	double thresh_monitor = 0;
	double max_dist = 10;
	// TO DO: remove hard code
	boost_sleep_ms(3000);

	for (;;) {

		Mat img; imggray.copyTo(img);// Make local copy of image for resizing

		Mat Pixel_change; Diff_img.copyTo(Pixel_change);

		imshow("Diff", Pixel_change);
		waitKey(1);

		switch (worm_finding_mode) {
		case 0:

			if (!findWormNearPickupLoc) {
				SearchWormWholeImg(SingleFrameNet, MultiFrameNet, img, Pixel_change, dsfactor, conf_thresh, energy_thresh, norm_type, alpha, beta, gamma);
			}
			else { 
				SearchWormNearPickupLoc(SingleFrameNet, MultiFrameNet, img, Pixel_change, dsfactor, thresh_find_near_pickuploc, energy_thresh, norm_type, alpha, beta, gamma); 
			}

			//SearchWorm(SingleFrameNet, MultiFrameNet, img, Pixel_change, dsfactor, conf_thresh, energy_thresh, norm_type, alpha, beta, gamma);
			//SearchWorm(ctrlPanel);
			//SearchWormsAmount(ctrlPanel);
			break;
		case 1:
			if (!trackwormNearPcikupLoc) {
				MonitorWormROIs(HighEnergyCentroids, radius_Roi, img, Pixel_change, SingleFrameNet, dsfactor, thresh_monitor, norm_type, max_dist, alpha_mon, beta_mon, gamma_mon);
				//MonitorWormRoiGaus(HighEnergyCentroids, radius_Roi, img, Pixel_change, SingleFrameNet, dsfactor, thresh_monitor, norm_type, max_dist, alpha_mon, beta_mon, gamma_mon);
			}
			else {
				MonitorWormROIs(HighEnergyCentroids, radius_roi_small, img, Pixel_change, SingleFrameNet, dsfactor, thresh_monitor, norm_type, max_dist, alpha_mon, beta_mon, gamma_mon);
			}

			//MonitorWormROIs(HighEnergyCentroids, radius_Roi, img, Pixel_change, SingleFrameNet, dsfactor, thresh_monitor, norm_type, max_dist, alpha_mon, beta_mon, gamma_mon);

			break;
		default:
			//SearchWormWholeImg(SingleFrameNet, MultiFrameNet, img, Pixel_change, dsfactor, conf_thresh, energy_thresh, norm_type, alpha, beta, gamma);
			SearchWorm(ctrlPanel); 
			//SearchWorm(SingleFrameNet, MultiFrameNet, img, Pixel_change, dsfactor, conf_thresh, energy_thresh, norm_type, alpha, beta, gamma);
		}

		// Check for exit
		boost_interrupt_point();
	}
}


double totalmass(std::vector<double> vec_mass) {
	double sum_mass = 0;
	for (int i = 0; i < vec_mass.size(); i++) {
		sum_mass += vec_mass[i];
	}
	return sum_mass;
}


cv::Point2d CenterofMass(std::vector<cv::Point> vec_pts, std::vector<double> vec_probs) {

	cv::Point2d center(-1, -1);

	if ((vec_pts.size() == vec_probs.size()) && vec_pts.size() > 0) {

		double weighted_sum_x = 0;
		double weighted_sum_y = 0;
		for (int i = 0; i < vec_pts.size(); i++) {
			weighted_sum_x += vec_probs[i] * vec_pts[i].x;
			weighted_sum_y += vec_probs[i] * vec_pts[i].y;
		}

		if (totalmass(vec_probs) != 0) {
			double center_x = weighted_sum_x / totalmass(vec_probs);
			double center_y = weighted_sum_y / totalmass(vec_probs);
			center.x = center_x;
			center.y = center_y;
		}
		else {
			// cout << "Fail to calculate center of mass: sum of mass is 0" << endl;
		}

	}
	else {
		cout << "Fail to calculate center of mass: empty vector or unequal vector size" << endl;
	}

	return center;
}

// Calculate the coordinate of each bin
void CalBinCoor(vector<Point>& ds_coords, vector<Point>& coords, Size sz, double dsfactor) {
	// ds_coords: downsampled relative bin coord relative to ROI
	// coords: relative bin coord relative to ROI
	ds_coords.clear();
	coords.clear();

	// Network designed for 20x20 image
	// Thus, Coordinates of bin are fixed once a ROI is defined (size can vary)
	int radius = 10, jump = 5;

	Size dsRoi_sz = Size(round(dsfactor*sz.width), round(dsfactor*sz.height));

	for (int r = radius; r < dsRoi_sz.height - radius - 1; r += jump) {
		for (int c = radius; c < dsRoi_sz.width - radius - 1; c += jump) {
			ds_coords.push_back(Point(c, r));
			coords.push_back(Point(c, r) / dsfactor);
		}
	}
}

// Points closed to each other are removed, represented by a point that has the highest probability among them
void RemoveCloseWorms(vector<Point>& worm_coords, vector<double>& prob_coords, double dist) {

	for (int i = 0; i < size(worm_coords); i++) {
		for (int j = i + 1; j < size(worm_coords); j++) {
			double this_dist = ptDist(worm_coords[i], worm_coords[j]);
			if (this_dist < dist) {
				if (prob_coords[i] > prob_coords[j]) {
					worm_coords[j] = Point(-1, -1);
					prob_coords[j] = -999;
				}
				else {
					worm_coords[i] = Point(-1, -1);
					prob_coords[i] = -999;
				}
			}
		}
	}

	// Remove the points that are (-1, -1)
	worm_coords.erase(remove(worm_coords.begin(), worm_coords.end(), Point(-1, -1)), worm_coords.end());
	// Remove the values that are -999
	prob_coords.erase(remove(prob_coords.begin(), prob_coords.end(), -999), prob_coords.end());

}


void Worms::ParalWormFinder(const cv::Mat& cur_frm, const cv::Mat& pre_frm, const ControlPanel CtrlPanel, std::vector<cv::Point>& worm_posit, std::vector<double>& vec_ergy, 
								  std::vector<cv::Mat>& diff_img, std::vector<cv::Mat>& thres_img) 
{


	// Iterate through ROI images
	for (int i = 0; i < CtrlPanel.ROIs.size(); i++) {

		// Get Global coordinate, width and height of ROI
		int this_roi_x = CtrlPanel.ROIs[i].x;
		int this_roi_y = CtrlPanel.ROIs[i].y;
		int this_roi_width = CtrlPanel.ROIs[i].width;
		int this_roi_height = CtrlPanel.ROIs[i].height;

		// Get the ROI image
		Mat Roi_img;
		cur_frm(CtrlPanel.ROIs[i]).copyTo(Roi_img);

		// Divide to bins

		// Declare vectors that hold coordinates of each bin
		// Network designed for 20x20 pixels image
		// Thus, Coordinates of bin center are fixed once a ROI is defined (size of bin can vary)
		vector<Point> ds_coords, coords;
		CalBinCoor(ds_coords, coords, Roi_img.size(), CtrlPanel.dsfactor);

		// Declare vectors that holds different energy components of each bin
		vector<double> total_ergy, mvm_ergy, conf_ergy, inten_ergy;
		for (int j = 0; j < ds_coords.size(); j++) { 
			total_ergy.push_back(0);
			mvm_ergy.push_back(0);
			conf_ergy.push_back(0);
			inten_ergy.push_back(0);
		}

		// Processing Selector
		// Energy = alpha * Distance to pickup Loc + beta * (pixel change - mean) / SD + gamma * (Confidence - mean) / SD + delta * Intensity classification score
		if (CtrlPanel.beta != 0) {

			// Compute difference image
			Mat Roi_pre_img;
			pre_frm(CtrlPanel.ROIs[i]).copyTo(Roi_pre_img);
			Mat Pixel_change;
			absdiff(Roi_img, Roi_pre_img, Pixel_change);

			if (CtrlPanel.thres_diff_img > 0) { threshold(Pixel_change, Pixel_change, CtrlPanel.thres_diff_img, 255, THRESH_BINARY); }

			// Copy to output ROI difference image vector
			diff_img.push_back(Pixel_change);

			// Feed to motion detector
			vector<int> vec_pix = measureBinPixDiff(coords, Pixel_change, CtrlPanel.rad_bin_motion, CtrlPanel.get_bin_mot);

			// Add the motion energy component
			for (int m = 0; m < vec_pix.size(); m++) {
				mvm_ergy[m] = CtrlPanel.beta * ((vec_pix[m] - CtrlPanel.miu_pix) / CtrlPanel.sd_pix);
			}
		}

		if (CtrlPanel.gamma != 0) {
			// Feed to CNN
			Mat img_out; // Shouldn't get used because out_img_mode defaults to 0
			//CnnOutput out_roi = measureBinConf(ds_coords, Roi_img, CtrlPanel.net, CtrlPanel.dsfactor, CtrlPanel.rad_bin_cnn);
			CnnOutput out_roi = findWormsInImage(Roi_img, CtrlPanel.net, CtrlPanel.dsfactor, img_out, false);

			// Add the CNN energy component
			for (int n = 0; n < out_roi.probs.size(); n++) {
				conf_ergy[n] = CtrlPanel.gamma * ((out_roi.probs[n] - CtrlPanel.miu_conf) / CtrlPanel.sd_conf);
			}
		}

		if (CtrlPanel.delta != 0) {
			// Feed to Intensity classifier (binary etc.)
			vector<double> vec_inten_class = measureIntenBinClass(coords, Roi_img, CtrlPanel.rad_bin_thres, CtrlPanel.inten_thresh, thres_img, CtrlPanel.blursize, CtrlPanel.smallsizetoremove, CtrlPanel.get_bin_class);

			// Add the intensity classification score
			for (int k = 0; k < vec_inten_class.size(); k++) {
				inten_ergy[k] = CtrlPanel.delta * vec_inten_class[k];
			}
		}

		// Calculate the total energy and maximum energy
		double max_energy = -99999;
		int max_idx = -1;
		for (int i = 0; i < total_ergy.size(); i++) {
			total_ergy[i] = mvm_ergy[i] + conf_ergy[i] + inten_ergy[i];

			if (total_ergy[i] > max_energy) {
				max_energy = total_ergy[i];
				max_idx = i;
			}
		}


		// Representation
		ControlPanel::Repres_mode Rep_mode = (ControlPanel::Repres_mode)(CtrlPanel.Rep_idx);
		if (Rep_mode == ControlPanel::MULT) {
			// Multiple representation
			// Show all the bins that have energy value higher than a threshold

			// Get high energy bin
			for (int i = 0; i < total_ergy.size(); i++) {
				if (total_ergy[i] > CtrlPanel.energy_thresh
					&& mvm_ergy[i] >= CtrlPanel.ergy_thres_motion
					//&& conf_ergy[i] >= CtrlPanel.ergy_thres_cnn
					//&& inten_ergy[i] >= CtrlPanel.ergy_thres_inten
					) // Energy thresholder
				{

					// Convert the global coordinate of bin respect to the whole image 
					Point gbl_bin;
					gbl_bin.x = this_roi_x + coords[i].x;
					gbl_bin.y = this_roi_y + coords[i].y;

					// Append to worm position and total energy vector
					worm_posit.push_back(gbl_bin);
					vec_ergy.push_back(total_ergy[i]);

				}
			}

		}
		else if (Rep_mode == ControlPanel::SIN) {
			// Single representation

			// Find the center of mass of energy value of bins in ROI
			Point2d ctr_mass = CenterofMass(coords, total_ergy);
			
			// Convert to global coordinate
			Point gbl_ctr_mass;
			gbl_ctr_mass.x = this_roi_x + ctr_mass.x;
			gbl_ctr_mass.y = this_roi_y + ctr_mass.y;

			// Append to worm position vector
			if (total_ergy[max_idx] > CtrlPanel.ergy_thres_mon) { worm_posit.push_back(gbl_ctr_mass); }
			// If the maximum energy of this Roi cannot reach a threshold, then append the center of this Roi
			else { worm_posit.push_back(Point(this_roi_x + this_roi_width / 2, this_roi_y + this_roi_height / 2)); }

			// In case of empty vector
			if (max_idx != -1) vec_ergy.push_back(total_ergy[max_idx]);
			else vec_ergy.push_back(0);
		}
	}
}

std::vector<std::vector<cv::Point>> Worms::WormTrajectory(std::vector<cv::Mat>& vec_frms, const std::vector<cv::Point> initial_pts) {

	// Flip the sequence of frames (reverse the time)
	reverse(vec_frms.begin(), vec_frms.end());

	// vector contining the worm positions at each frame
	vector<vector<Point>> worm_traj;

	if (!initial_pts.empty()) {
		worm_traj.push_back(initial_pts); // Append the initial positions

		// Iterate each frame to find the worm position under tracking mode
		ControlPanel CP1(ctrlPanel);

		for (int i = 0; i < vec_frms.size() - 1; i++) {

			// Debug show frame for debug

			//Mat debug_img;
			//vec_frms[i].copyTo(debug_img);
			//for (int j = 0; j < worm_traj[i].size(); j++) { drawCross(debug_img, worm_traj[i][j], colors::rr, 1, 8); }
			//namedWindow("Display frame 2", CV_WINDOW_AUTOSIZE);
			//imshow("Display frame 2", debug_img);
			//waitKey(1);

			setDmyTrajTracker(CP1, worm_traj[i]);
			worm_traj.push_back(DmyTrajTracker(CP1, vec_frms[i + 1]));

			// boost_sleep_ms(70);

			
		}
	}

	return worm_traj;
}



std::vector<cv::Point> Worms::DmyTrajTracker(const ControlPanel& CtrlPanel, const cv::Mat& frm) {

	// Pass the ContrlPanel object to Parallel worm finder
	vector<Point> worm_pos;
	vector<double> worm_ergy;
	vector<Mat> pix_diff, thres_img; // Dummy

	ParalWormFinder(frm, frm, CtrlPanel, worm_pos, worm_ergy, pix_diff, thres_img);

	// Output the worm positions
	return worm_pos;
}

void Worms::setDmyTrajTracker(ControlPanel& CP, std::vector<cv::Point> last_pts) {

	// Construct ROIs from last frame

	CP.ROIs.clear();

	// Calculate the ROIs from worms' position seen at the last frame
	getROIsfromPts(last_pts, CP.sz_full_img, CP.ROIs, CP.rad_roi);

	// Weights
	CP.beta = 0; CP.gamma = 1; CP.delta = 0;
	CP.miu_conf = 0; CP.sd_conf = 1;
	CP.ergy_thres_mon = CP.gamma * (4 - CP.miu_conf) / CP.sd_conf;

	// Representation mode
	CP.Rep_idx = 1;

}

void Worms::MainCamWormFinder(cv::Size sz, std::string fonnx_worm) {

	boost_sleep_ms(3000);
	int search_rad = 70;

	// Create different ControlPanel objects for different applications
	ControlPanel CP0(sz, fonnx_worm); // Default
	ControlPanel CP1(sz, fonnx_worm); setMainCamTrackWormRoi(CP1);
	ControlPanel CP2(sz, fonnx_worm); setMainCamFindWormContr(CP2); ///*********************************** CP2 is not used, it was commented out where it would have been
	ControlPanel CP3(sz, fonnx_worm); setMainCamSegmWormRoi(CP3, CP1.ROIs);
	ControlPanel CP4(sz, fonnx_worm); setMainCamFindWormNearPickupLoc(CP4, search_rad);

	for (;;) {

		// showDebugImage(imggray(ovlpFov), false, "overlap", 1, false);

		if (worm_finding_mode == 0) {
			
			MainCamFindWormRoi(CP0);
		
		}
		else if (worm_finding_mode == 1) {
			
			setMainCamTrackWormRoi(CP1);
			MainCamTrackWormRoi(CP1);

			// TO DO: think the coherence between above; empty centroids/rois; add the vector of thresholded image in the parallel worm finder
			// setMainCamSegmWormRoi(CP3, CP1.ROIs);
			// MainCamSegmWormRoi(CP3);

		}
		else if (worm_finding_mode == 2) {
			// cout << "worm_finding_mode = " << worm_finding_mode << endl;
			setMainCamFindWormNearPickupLoc(CP4, search_rad);
			MainCamFindWormRoi(CP4);
		}


		//switch (worm_finding_mode) {
		//case 0:
		//	cout << "worm_finding_mode = " << worm_finding_mode << endl;
		//	MainCamFindWormRoi(CP0);
		//	
		//	break;
		//case 1:
		//	cout << "worm_finding_mode = " << worm_finding_mode << endl;
		//	setMainCamTrackWormRoi(CP1);
		//	MainCamTrackWormRoi(CP1);

		//	// TO DO: think the coherence between above; empty centroids/rois; add the vector of thresholded image in the parallel worm finder
		//	// setMainCamSegmWormRoi(CP3, CP1.ROIs);
		//	// MainCamSegmWormRoi(CP3);

		//	break;

		//case 2:
		//	cout << "worm_finding_mode = " << worm_finding_mode << endl;
		//	setMainCamFindWormNearPickupLoc(CP4, 50);
		//	MainCamFindWormRoi(CP4);

		//	break;


		//default:
		//	MainCamFindWormRoi(CP0);
		//}

		// Find worm contour in overlap FOV with fluo cam
		// MainCamFindWormContr(CP2);


		// Check for exit
		boost_interrupt_point();


	}

}


void Worms::MainCamFindWormRoi(const ControlPanel CtrlPanel) {

	// Reset track
	Monitored_ROIs.clear();
	resetTrack();

	// Pass the ContrlPanel object to Parallel worm finder
	vector<Point> worm_pos;
	vector<double> worm_ergy;
	vector<Mat> pix_diff, thres_img; // Dummy


	ParalWormFinder(imggray, pre_frame_lo_mag, CtrlPanel, worm_pos, worm_ergy, pix_diff, thres_img);

	// Points closed to each other are removed, represented by a point that has the highest energy among them
	if (worm_pos.size() < 50) {
		RemoveCloseWorms(worm_pos, worm_ergy, 40);
		// Update the centroids of worm or tracked worm
		assigncentroidAll(worm_pos);
	}
	else { assigncentroidAll({pickup_location});}

}


void Worms::setMainCamTrackWormRoi(ControlPanel& CP) {

	// Construct ROIs from last frame

	CP.ROIs.clear();
	// If monitor_rois is empty, calculate the ROIs from centroidAll
	if (!trackwormNearPcikupLoc) {
		getROIsfromPts(centroidAll, CP.sz_full_img, CP.ROIs, CP.rad_roi);
	} 
	else{
		vector<Point> dummy_pt_vec({ pickup_location });
		getROIsfromPts(dummy_pt_vec, CP.sz_full_img, CP.ROIs, CP.rad_roi);
	}


	// Weights
	CP.beta = 0; CP.gamma = 1; CP.delta = 0;
	CP.miu_conf = 0; CP.sd_conf = 1;
	CP.ergy_thres_mon = CP.gamma * (4 - CP.miu_conf) / CP.sd_conf;

	// Representation mode
	CP.Rep_idx = 1;


}

int getclosestPoint(std::vector<cv::Point> pts_list, cv::Point ref_pt) {

	int idx = -1;
	double min_dist = 99999;
	for (int i = 0; i < pts_list.size(); i++) {
		if (ptDist(ref_pt, pts_list[i]) < min_dist) {
			min_dist = ptDist(ref_pt, pts_list[i]);
			idx = i;
		}
	}

	return idx;
}


void Worms::MainCamTrackWormRoi(const ControlPanel CtrlPanel) {


	// Pass the ContrlPanel object to Parallel worm finder
	vector<Point> worm_pos;
	vector<double> worm_ergy;
	vector<Mat> pix_diff, thres_img; // Dummy

	ParalWormFinder(imggray, pre_frame_lo_mag, CtrlPanel, worm_pos, worm_ergy, pix_diff, thres_img);


	// Update ROIs to monitor for next frame
	vector<Rect> Rois;
	getROIsfromPts(worm_pos, CtrlPanel.sz_full_img, Rois, CtrlPanel.rad_roi);
	UpdateMonitorRois(Rois);
	assigncentroidAll(worm_pos);

	// Get the point that are closest to the pick-up loc
	int idx = getclosestPoint(worm_pos, pickup_location);

	// Set the track worm
	if (idx != -1) { // if worm_pos is not empty
		tracked_worm.reset_centroid(worm_pos[idx]);
		tracked_worm.reset_roi(Rois[idx]);

		// add an option
		if (!trackMultipleWorm) { // If track single worm, pick the one closest to the pickup location to track
			UpdateMonitorRois({ Rois[idx] });
			assigncentroidAll({ worm_pos[idx] });
		}
	}
	else {
		resetTrack();
		cout << "No high energy centroids found, press w to refind" << endl;
		boost_sleep_ms(500);
	}
	
}


void Worms::setMainCamSegmWormRoi(ControlPanel& CP, vector<Rect> Rois) {

	// Get the ROIs
	CP.ROIs.clear();
	for (int i = 0; i < Rois.size(); i++) {
		CP.ROIs.push_back(Rois[i]);
	}

	// Select thresholder only
	CP.beta = 0; CP.gamma = 0; CP.delta = 1;

	// Blur size and size of blobs to remove
	CP.blursize = cv::Size(3, 3);
	CP.smallsizetoremove = cv::Size(3, 3);

	// Set intensity threshold
	CP.inten_thresh = 70;

}

///******************************************************************* Used in MainCamWormFinder but is commented out
void Worms::MainCamSegmWormRoi(const ControlPanel CtrlPanel) {

	// Pass the ContrlPanel object to Parallel worm finder
	vector<Point> worm_pos;
	vector<double> worm_ergy;
	vector<Mat> pix_diff; // Dummy

	// Vector of thresholded ROI images
	vector<Mat> vec_thres_cur_frm;
	ParalWormFinder(imggray, pre_frame_lo_mag, CtrlPanel, worm_pos, worm_ergy, pix_diff, vec_thres_cur_frm);

	// Get the largest blob for each ROI image
	for (int i = 0; i < vec_thres_cur_frm.size(); i++) {
		isolateLargestBlob(vec_thres_cur_frm[i], 1);
	}

	// Update worms' contours
	vector<vector<Point>> cturs;
	for (int t = 0; t < vec_thres_cur_frm.size(); t++) {

		vector<vector<Point>> contoursTemp;
		vector<Vec4i> hierarchy;
		findContours(vec_thres_cur_frm[t], contoursTemp, hierarchy, RETR_CCOMP, CHAIN_APPROX_SIMPLE);

		// convert to to global coordinate
		if (contoursTemp.size() > 0) {
			for (int i = 0; i < contoursTemp[0].size(); i++) {
				(contoursTemp[0])[i].x += CtrlPanel.ROIs[t].x;
				(contoursTemp[0])[i].y += CtrlPanel.ROIs[t].y;
			}

			cturs.push_back(contoursTemp[0]);
		}
	}

	UpdatecontoursAll(cturs);
	
	// Update worms's area
	UpdateAreaAll();
}


///******************************************************************* Only used in MainCamWormFinder, but the object it is used to create is not used (because its commented out)
void Worms::setMainCamFindWormContr(ControlPanel& CP) {

	// Set the Roi to be the overlapped FOV between main and fluo camera
	CP.ROIs.clear();
	CP.ROIs.push_back(ovlpFov);

	// Select thresholder only
	CP.beta = 0; CP.gamma = 0; CP.delta = 1;

	// Blur size and size of blobs to remove
	CP.blursize = cv::Size(3, 3);
	CP.smallsizetoremove = cv::Size(3, 3);

	// Set intensity threshold
	CP.inten_thresh = 70;
}

///******************************************************************* Only used in MainCamWormFinder (and even then is commented out), which is not used
void Worms::MainCamFindWormContr(const ControlPanel CtrlPanel) {

	// Dummy vectors, we dont need it
	vector<Point> worm_pos;
	vector<double> worm_ergy;
	vector<Mat> pix_diff;

	// Get the thresholded current frame
	vector<Mat> vec_thres_cur_frm;
	ParalWormFinder(imggray, pre_frame_lo_mag, CtrlPanel, worm_pos, worm_ergy, pix_diff, vec_thres_cur_frm);

	// Get the largest blob
	Mat thres_cur_frm; vec_thres_cur_frm[0].copyTo(thres_cur_frm);
	isolateLargestBlob(thres_cur_frm, 1);
	// showDebugImage(thres_cur_frm, false, "thres", 1, false);

	// Evaluate the geometry of blob
	vector<vector<Point>> contoursTemp;
	vector<Vec4i> hierarchy;
	findContours(thres_cur_frm, contoursTemp, hierarchy, RETR_CCOMP, CHAIN_APPROX_SIMPLE);

	// Update the contour of tracked worm
	if (!contoursTemp.empty()) {
		tracked_worm.updateContourV2(contoursTemp[0]);
	}

	 //Measure geometry of worm blob
	for (int i = 0; i < contoursTemp.size(); i++) {
		double A = contourArea(contoursTemp[i]);		// Area
		double P = arcLength(contoursTemp[i], true);	// Perimeter
		double L = P / 2;								// Approximate length 
		double W = A / L;								// Approximate width
		double LW = L / W;								// Length-to-Width ratio

		//cout << "Area = " << A << endl;
		//cout << "Perimeter = " << P << endl;
		//cout << "Approx. length under MainCam = " << L << endl;
		//cout << "Approx. width = " << W << endl;
		//cout << "Len-to-Wid ratio = " << LW << endl;

	}
}

void Worms::setMainCamFindWormNearPickupLoc(ControlPanel& CP, int rad) {

	// Set the searching region as the a bounding box whose center point is the unloading position
	Point unload_pt = getPickupLocation();
	Rect ROI;
	ROI.x = unload_pt.x - rad;
	ROI.y = unload_pt.y - rad;
	ROI.width = 2 * rad;
	ROI.height = 2 * rad;

	// Prevent OOB
	ROI.x = max(ROI.x, 0);
	ROI.y = max(ROI.y, 0);
	ROI.width = min(ROI.width, CP.sz_full_img.width - ROI.x);
	ROI.height = min(ROI.height, CP.sz_full_img.height - ROI.y);

	// Append to control panel object
	CP.ROIs.clear();
	CP.ROIs.push_back(ROI);
	unload_search_Roi = ROI;

}

///******************************************************************* Only used in TestScripts.cpp
void Worms::setFluoCamSegWormWholeFOV(ControlPanel& CP) {
	// Select thresholder only
	CP.beta = 0; CP.gamma = 0; CP.delta = 1;

	// blurring and blob errosion
	CP.blursize = cv::Size(25, 25);
	CP.smallsizetoremove = cv::Size(0, 0);
	CP.inten_thresh = 100;
}


///******************************************************************* Not used
void Worms::setFluoCamGetFluoImg(ControlPanel& CP) {

	// Select thresholder only
	CP.beta = 0; CP.gamma = 0; CP.delta = 1;

	// Disable blurring and blob errosion
	CP.blursize = cv::Size(1, 1);
	CP.smallsizetoremove = cv::Size(0, 0);
	CP.inten_thresh = 50;
}


///******************************************************************* Only used in FindWormEnergy, which is commented out where used
void Worms::SearchWormWholeImg(const cv::dnn::Net SingleFrameNet, const cv::dnn::Net MultiFrameNet, cv::Mat& imggray, cv::Mat& DiffImg,
								     double dsfactor, double conf_thresh, double energy_thresh, int norm_type, double alpha, double beta, double gamma)
{
	Monitored_ROIs.clear();

	resetTrack();

	// Dummy imgout, we don't need it
	Mat imgout;

	// Run CNN to get output
	CnnOutput out = findWormsInImage(imggray, SingleFrameNet, dsfactor, imgout, false, 0, 0, conf_thresh, norm_type);

	// Get the centroids of bins that contain worms
	vector<Point> worm_coords;
	vector<double> prob_coords;

	for (int i = 0; i < size(out.classes); i++)
	{
		if (out.classes[i] == 0) {
			worm_coords.push_back(out.coords[i] / dsfactor);
			prob_coords.push_back(out.probs[i]);
		}
	}

	// Points closed to each other are removed, represented by a point that has the highest probability among them
	RemoveCloseWorms(worm_coords, prob_coords, 20);

	// Assign the coords to worms
	assigncentroidAll(worm_coords);

	// Update confidence value of centroid
	UpdateConfiSinglefrmCnn(prob_coords);

	// Update centroids distance to pick up location
	UpdateCentroidDist();

	// Update the pixel changes in ROI
	assignPixDiffRoi(RoiPixelDiff(DiffImg, centroidAll));

	// Calculate the energy functional
	CalCentroidEnergy(alpha, beta, gamma);

	// Remove the centroid having energy lower than threshold
	GetHighEnergyCentroid(energy_thresh);
}


///******************************************************************* Only used in FindWormEnergy, which is commented out where used
void Worms::SearchWormNearPickupLoc(const cv::dnn::Net SingleFrameNet, const cv::dnn::Net MultiFrameNet, cv::Mat& imggray, cv::Mat& DiffImg,
										  double dsfactor, double conf_thresh, double energy_thresh, int norm_type, double alpha, double beta, double gamma) 
{
	Monitored_ROIs.clear();

	resetTrack();

	Mat Roi_img;
	imggray(pickup_Roi).copyTo(Roi_img);

	// Dummy imgout, we don't need it
	Mat imgout;

	// Run CNN to get output
	CnnOutput out_roi = findWormsInImage(Roi_img, SingleFrameNet, dsfactor, imgout, false, 0, 0, conf_thresh, norm_type);

	// Get the centroids of bins that contain worms
	vector<Point> worm_coords;
	vector<double> prob_coords;

	for (int i = 0; i < size(out_roi.classes); i++)
	{
		if (out_roi.classes[i] == 0) {
			Point coord_Roi = out_roi.coords[i] / dsfactor;
			Point coord_whole_img = Point(pickup_Roi.x + coord_Roi.x, pickup_Roi.y + coord_Roi.y);
			worm_coords.push_back(coord_whole_img);
			prob_coords.push_back(out_roi.probs[i]);
		}
	}

	// Points closed to each other are removed, represented by a point that has the highest probability among them
	RemoveCloseWorms(worm_coords, prob_coords, 20);

	// Assign the coords to worms
	assigncentroidAll(worm_coords);

	// Update confidence value of centroid
	UpdateConfiSinglefrmCnn(prob_coords);

	// Update centroids distance to pick up location
	UpdateCentroidDist();

	// Update the pixel changes in ROI
	assignPixDiffRoi(RoiPixelDiff(DiffImg, centroidAll));

	// Calculate the energy functional
	CalCentroidEnergy(alpha, beta, gamma);

	// Remove the centroid having energy lower than threshold
	GetHighEnergyCentroid(energy_thresh);
}

///******************************************************************* Only used in FindWormEnergy, which is commented out where used
void Worms::SearchWorm(const cv::dnn::Net SingleFrameNet, const cv::dnn::Net MultiFrameNet, cv::Mat& imggray, cv::Mat& DiffImg, 
							 double dsfactor, double conf_thresh, double energy_thresh, int norm_type, double alpha, double beta, double gamma) 
{
	Monitored_ROIs.clear();

	resetTrack();

	Mat Roi_img;
	imggray(search_roi).copyTo(Roi_img);

	// Dummy imgout, we don't need it
	Mat imgout;

	// Run CNN to get output
	CnnOutput out_roi = findWormsInImage(Roi_img, SingleFrameNet, dsfactor, imgout, false, 0, 0, conf_thresh, norm_type);

	// Get the centroids of bins that contain worms
	vector<Point> worm_coords;
	vector<double> prob_coords;
	
	for (int i = 0; i < size(out_roi.classes); i++)
	{
		if (out_roi.classes[i] == 0) {
			Point coord_Roi = out_roi.coords[i];
			Point coord_whole_img = Point(search_roi.x + coord_Roi.x, search_roi.y + coord_Roi.y);
			worm_coords.push_back(coord_whole_img);
			prob_coords.push_back(out_roi.probs[i]);
		}
	}

	// Points closed to each other are removed, represented by a point that has the highest probability among them
	RemoveCloseWorms(worm_coords, prob_coords, 20);

	// Assign the coords to worms
	assigncentroidAll(worm_coords);

	// Update confidence value of centroid
	UpdateConfiSinglefrmCnn(prob_coords);

	// Update centroids distance to pick up location
	UpdateCentroidDist();

	// Update the pixel changes in ROI
	assignPixDiffRoi(RoiPixelDiff(DiffImg, centroidAll));

	// Calculate the energy functional
	CalCentroidEnergy(alpha, beta, gamma);

	// Remove the centroid having energy lower than threshold
	GetHighEnergyCentroid(energy_thresh);
}

///******************************************************************* Only used in FindWormEnergy, which is commented out where used
void Worms::SearchWorm(const ControlPanel CtrlPanel) {

	Monitored_ROIs.clear();

	resetTrack();

	Mat Roi_img;
	imggray(CtrlPanel.search_ROI).copyTo(Roi_img);

	// Dummy imgout, we don't need it
	Mat imgout;

	// Run CNN to get output
	CnnOutput out_roi = findWormsInImage(Roi_img, CtrlPanel.net, CtrlPanel.dsfactor, imgout, false, 0, 0, CtrlPanel.conf_thresh, CtrlPanel.norm_type);

	// Get the centroids of bins that contain worms
	vector<Point> worm_coords;
	vector<double> prob_coords;

	for (int i = 0; i < size(out_roi.classes); i++)
	{
		if (out_roi.classes[i] == 0) { // Class 0 represents worm
			Point coord_Roi = out_roi.coords[i] / (double) CtrlPanel.dsfactor;
			Point coord_whole_img = Point(CtrlPanel.search_ROI.x + coord_Roi.x, CtrlPanel.search_ROI.y + coord_Roi.y);
			worm_coords.push_back(coord_whole_img);
			prob_coords.push_back(out_roi.probs[i]);
		}
	}

	// Points closed to each other are removed, represented by a point that has the highest probability among them
	RemoveCloseWorms(worm_coords, prob_coords, 20);

	// Assign the coords to worms
	assigncentroidAll(worm_coords);

	// Update confidence value of centroid
	UpdateConfiSinglefrmCnn(prob_coords);

	// Update centroids distance to pick up location
	UpdateCentroidDist();

	// Update the pixel changes in ROI
	assignPixDiffRoi(RoiPixelDiff(Diff_img, centroidAll));

	// Calculate the energy functional
	CalCentroidEnergy(CtrlPanel.alpha, CtrlPanel.beta, CtrlPanel.gamma);

	// Remove the centroid having energy lower than threshold
	GetHighEnergyCentroid(CtrlPanel.energy_thresh);

}

///******************************************************************* Only used in FindWormEnergy, which is commented out where used
void Worms::SearchWormsAmount(const ControlPanel CtrlPanel) {

	Monitored_ROIs.clear();

	resetTrack();
	imggray(CtrlPanel.search_ROI);
	Mat Roi_img;
	imggray(CtrlPanel.search_ROI).copyTo(Roi_img);

	// Dummy imgout, we don't need it
	Mat imgout;

	// Run CNN to get output
	CnnOutput out_roi = findWormsInImage(Roi_img, CtrlPanel.net, CtrlPanel.dsfactor, imgout, false, 0, 0, CtrlPanel.conf_thresh, CtrlPanel.norm_type);

	// Get the centroids of bins that contain worms
	vector<Point> worm_coords;
	vector<double> prob_coords;
	int use;
	(CtrlPanel.bin_amount == ALL) ? use = size(out_roi.classes) : use = CtrlPanel.bin_amount;
	for (int i = 0; i < size(out_roi.classes); i++)
	{
		if (worm_coords.size() > use) {
			break;
		}
		if (out_roi.classes[i] == 0) { // Class 0 represents worm
			Point coord_Roi = out_roi.coords[i] / (double)CtrlPanel.dsfactor;
			Point coord_whole_img = Point(CtrlPanel.search_ROI.x + coord_Roi.x, CtrlPanel.search_ROI.y + coord_Roi.y);
			worm_coords.push_back(coord_whole_img);
			prob_coords.push_back(out_roi.probs[i]);
		}
	}

	// Points closed to each other are removed, represented by a point that has the highest probability among them
	RemoveCloseWorms(worm_coords, prob_coords, 20);

	// Assign the coords to worms
	assigncentroidAll(worm_coords);

	// Update confidence value of centroid
	UpdateConfiSinglefrmCnn(prob_coords);

	// Update centroids distance to pick up location
	UpdateCentroidDist();

	// Update the pixel changes in ROI
	assignPixDiffRoi(RoiPixelDiff(Diff_img, centroidAll));

	// Calculate the energy functional
	CalCentroidEnergy(CtrlPanel.alpha, CtrlPanel.beta, CtrlPanel.gamma);

	// Remove the centroid having energy lower than threshold
	GetHighEnergyCentroid(CtrlPanel.energy_thresh);

	//remove if one
	if (CtrlPanel.single_worm == true) {
		ChooseOneWorm(CtrlPanel);
	}
	

}


bool Worms::findOnClick(int x, int y) {
	double min_dist = 999; // minimum distance from a contour to pickup location
	int min_idx = 0;    //the index of the closest contour

	
	//find the closest worm to the click point
	for (int i = 0; i < all_worms.size(); i++) {
		double this_dist = ptDist(Point(x,y), all_worms[i].getWormCentroid());
		
		if (this_dist < min_dist) {
			min_dist = this_dist;
			min_idx = i;
		}
	}


	if (min_dist < track_radius) {
		tracked_worm.reset(all_worms[min_idx].getContour());
		return true;
	}
	else {
		return false;
	}
}

///******************************************************************* Not used
std::vector<cv::Point> Worms::centroidTrack(const WormShape &last_worm, int search_radius) {
	vector<Point> contour = last_worm.getContour();

	if (contour.empty()) {
		throw EmptyWormException();
	}

	for (int i = 0; i < all_worms.size(); i++) {
		if (ptDist(last_worm.getWormCentroid(), all_worms[i].getWormCentroid()) < search_radius) {
			return all_worms[i].getContour();
		}
	}

	throw WormLostException();
}



//choose a worm to track and eventually pick
//selects based on the surrounding pixel intensities of each contours
//higher pixel intensities means fewer worms/debris
///******************************************************************* Is used in ScriptActions.cpp, but is commented out.
bool Worms::chooseWorm(pick_mode mode) {

	switch (mode) {
	case MANUAL:
	{
		printf("Waiting for user to select a worm\n");
		// TODO - timeout timer?
		while (!isWormTracked()) {
			boost_sleep_ms(100);
		}
		printf("Worm selected, beginning pick attempt\n");
		return true;
	}
	case AUTO:
	{
		printf("Beginning automatic worm selection\n");
		
		// if there are no contours, do nothing
		if (all_worms.empty()) {
			printf("No worms found\n");
			return false;
		}

		// flow track all worms
		vector<WormShape> potential_tracks = all_worms;

		printf("Tracking %zd object(s)", potential_tracks.size());

		// flow track all the worms for 5 seconds
		AnthonysTimer t;

		Mat initial_frame = imggray.clone();

		while (t.getElapsedTime() < track_time) {

			for (int i = 0; i < potential_tracks.size(); i++) {
				try {
					//potential_tracks[i].updateContour(flowPass(imggray, potential_tracks[i]));
					//////////////////////////////////////////// TODO - DEPRECATED BY WORMSHAPE METHOD
				}
				catch (...) {
					// if the worm is lost, clear the WormShape
					potential_tracks[i].clear();
				}

			}

			boost_sleep_ms(100);
		}

		int max_val = 0;
		int max_idx = -1;

		//choose the worm with greatest image diff
		for (int i = 0; i < potential_tracks.size(); i++) {
			if (potential_tracks[i].isEmpty()) {
				continue;
			}
			
			Rect roi = potential_tracks[i].boundingBox();
			Mat diff = Mat(imggray, roi) - Mat(initial_frame, roi);
			
			imwrite("img1.jpg", Mat(imggray, roi));
			imwrite("img2.jpg", Mat(initial_frame, roi));
			imwrite("diff.jpg", diff);

			int sum = 0;
			
			for (int row = 0; row < diff.rows; ++row) {
				uchar* p = diff.ptr(row);
				for (int col = 0; col < diff.cols; ++col) {
					sum += abs(*p);
					p++;
				}
			}

			printf("diff:%d\n", sum);

			if (sum > max_val) {
				max_val = sum;
				max_idx = i;
			}

			boost_sleep_ms(100);
		}

		if (max_idx == -1) { // no worms were found
			printf("No worms found, searching for others\n");
			return false;
		}

		printf("Worm was found, beginning pick\n");
		tracked_worm.reset(potential_tracks[max_idx].getContour());
		return true;
	}
	case NONE: 
	{
		printf("Worm selection disabled, skipping step\n");
	}
	case DROP:
	{
		printf("Dropping worm");
	}
	}
	return true;
}

///******************************************************************* Is used in Wormpicker.cpp, but is commented out
void Worms::UpdateDeque(cv::Mat& thisframe) {
	ImageQue.erase(ImageQue.begin());
	ImageQue.push_back(thisframe);
}


//void SortedBlobs::isMoving() {
//
//	for (;;) {
//		vector<Point> vec_centroids;
//		AnthonysTimer T;
//
//		//Waiting until CNN generate a vector of centroids
//		const int wait_time = 2;
//		while (T.getElapsedTime() < 2 && vec_centroids.empty()) {
//			vec_centroids = getcentroidAll(); // Get the updated centroid point of worms from CNN
//		}
//
//		if (vec_centroids.empty()) { continue; } // continue if vector of centroid is empty
//
//		// Get the absolute pixel difference in differeent ROIs
//		assignPixDiffRoi(RoiPixelDiff(ImageQue.back(), ImageQue.front()));
//	
//		boost_interrupt_point();
//	}
//
//}

///********************************************************** Only used in methods in this file, none of which are used
vector<int> Worms::RoiPixelDiff(cv::Mat& Differeence_Img, std::vector<cv::Point> vec_point) {

	vector<int> pixel_diff_rois;

	// Construct ROIs according to centroid points
	for (int i = 0; i < vec_point.size(); i++) {

		Rect worm_roi;
		// Set the ROI of worm
		worm_roi.x = vec_point[i].x - track_mvm_radius;
		worm_roi.y = vec_point[i].y - track_mvm_radius;
		worm_roi.width = 2 * track_mvm_radius;
		worm_roi.height = 2 * track_mvm_radius;

		// Prevent ROI out of bound
		worm_roi.x = max(worm_roi.x, 0);
		worm_roi.y = max(worm_roi.y, 0);
		worm_roi.width = min(worm_roi.width, Differeence_Img.size().width - worm_roi.x);
		worm_roi.height = min(worm_roi.height, Differeence_Img.size().height - worm_roi.y);
		worm_roi.width = max(worm_roi.width, 2);
		worm_roi.height = max(worm_roi.height, 2);


		// difference image in ROI
		Mat Diff_img = Mat(Differeence_Img, worm_roi);

		// Calculate the absolute pixel difference in ROI
		int sum = 0;
		for (int row = 0; row < Diff_img.rows; ++row) {
			uchar* p = Diff_img.ptr(row);
			for (int col = 0; col < Diff_img.cols; ++col) {
				sum += abs(*p);
				p++;
			}
		}

		// Append the pixel changes value in the vector
		pixel_diff_rois.push_back(sum);
	}

	return pixel_diff_rois;
}

vector<int> Worms::measureBinPixDiff(std::vector<cv::Point>& coords, cv::Mat& Pixel_change, int bin_radius, bool get_bin_mot) {

	vector<int> pixel_diff_bins;

	// Construct crop ROIs according to coordinates of bins
	if (get_bin_mot) {
		for (int i = 0; i < coords.size(); i++) {

			// Set crop bounds
			int r0 = coords[i].y - bin_radius;
			int r1 = coords[i].y + bin_radius;
			int c0 = coords[i].x - bin_radius;
			int c1 = coords[i].x + bin_radius;

			// get crop image
			Mat crop_img = Pixel_change(Rect(Point(c0, r0), Point(c1, r1)));

			// Calculate the absolute pixel difference in crop ROI
			int sum = 0;
			for (int row = 0; row < crop_img.rows; ++row) {
				uchar* p = crop_img.ptr(row);
				for (int col = 0; col < crop_img.cols; ++col) {
					sum += abs(*p);
					p++;
				}
			}

			// Append the pixel changes value in the vector
			pixel_diff_bins.push_back(sum);
		}
	}

	return pixel_diff_bins;
}

std::vector<double> Worms::measureIntenBinClass(std::vector<cv::Point>& coords, cv::Mat& Roi_img, int bin_radius, int thresh, std::vector<cv::Mat>& bw, cv::Size blursz, cv::Size removesz, bool get_bin_class) {

	// Construct a thresholded image
	Mat thres_img;
	
	int sigma = 5;
	GaussianBlur(Roi_img, thres_img, blursz, sigma, sigma); // Gaussian blur
	// threshold(thres_img, thres_img, thresh, 255, THRESH_BINARY_INV); // TO DO: add an changable option in thresholding type, carried by parameter object
	threshold(thres_img, thres_img, thresh, 255, THRESH_OTSU);
	thres_img = ~thres_img;

	// Remove small blobs
	if (removesz.width > 3 && removesz.height > 3) {
		Mat kernel = getStructuringElement(MORPH_ELLIPSE, removesz);
		morphologyEx(thres_img, thres_img, MORPH_OPEN, kernel);
	}

	// Copy to output thresholded ROI image vector
	bw.push_back(thres_img);

	vector<double> class_score;

	// Construct crop ROIs according to coordinates of bins

	if (get_bin_class) {
		for (int i = 0; i < coords.size(); i++) {

			// Set crop bounds
			int r0 = coords[i].y - bin_radius;
			int r1 = coords[i].y + bin_radius;
			int c0 = coords[i].x - bin_radius;
			int c1 = coords[i].x + bin_radius;

			// get crop image
			Mat crop_img = thres_img(Rect(Point(c0, r0), Point(c1, r1)));

			// Check whether this bin contain (part of) worm
			double min, max;
			cv::minMaxLoc(crop_img, &min, &max);
			bool is_containworm = (max > 0);

			// Append the classification score in the vector
			if (is_containworm) {
				class_score.push_back(1);
			}
			else {
				class_score.push_back(0);
			}
		}
	}
	
	return class_score;
}

void Worms::resetTrack() {
	tracked_worm.clear();
}

///******************************************************************* Not used
bool Worms::wasWormPickedByBlob() {
	//search for worms in the pickup region
	//check if any of them are at the pickup location
	// Check for up to 2 seconds
	boost_sleep_ms(1000);
	AnthonysTimer Tw;

	int j = 0;
	while (Tw.getElapsedTime() < 4.0) {

		// Lock the SortedBlobs object while the objects are being evaluated.
		boost::lock_guard<boost::mutex> guard(mtx_);

		for (int i = 0; i < all_worms.size(); i++) {

			Point ctr = all_worms[i].getWormCentroid();
			vector<Point> this_contour = all_worms[i].getContour();

			if (ptDist(ctr, pickup_location) < 75) { //verify that the worm is in all_worms
				
				// If the worm is found near the pickup site, set it was tracked so we can see it 
				tracked_worm.reset(this_contour);
				return false;
			}
		}
		std::cout << "Try " << j << " worms = " << all_worms.size() << endl;
		j++;
		boost_sleep_ms(50);
	}

	// No matching worm found - return true
	return true;
}

/*
	Procedure for checking worms by diff:
	1. Create one ROI 75 pix around the pickup loc and another annulus of the same area
	2. Pause 1.5 sec to allow any sloshing liquids to settle
	3. Screen the max difference images for 1.5s gap
	4. if average activity in ROI > C x Average acitivity in annulus, declare there is a worm there
	
*/

///******************************************************************* Used in quickPick methods inside Wormpick.cpp, but none of those methods are used.
bool Worms::mvmtAtPickedSite() {

	// STEP 1: Generate equivalent area ROIs A = pi*r^2, sqrt(A/pi) = r
	diff_search_ri = 75.0 / 640.0 * imggray.cols;
	double A = 3.14159*pow(diff_search_ri, 2);
	diff_search_ro = sqrt(A / 3.14159 + pow(diff_search_ri,2));

	Mat mask_i = Mat::zeros(Size(imggray.cols, imggray.rows), CV_8U);
	Mat mask_o = Mat::zeros(Size(imggray.cols, imggray.rows), CV_8U);

	/// Inner mask
	circle(mask_i, pickup_location, diff_search_ri, colors::ww,-1);

	/// Outer mask (set inner to black)
	circle(mask_o, pickup_location, diff_search_ro, colors::ww, -1);
	circle(mask_o, pickup_location, diff_search_ri, colors::kk, -1);

	// STEP 2: Pause to allow any liquids / background stuff to settle
	boost_sleep_ms(1200);

	// STEP 3: Screen the max difference for a 1.5s gap
	watching_for_picked = START_WATCHING; /// Sets the reference image
	boost_sleep_ms(1500);

	// Step 4 determine whether activity inside was meaningfully more than outside
	double Act_i = cv::mean(imggray, mask_i)[0]; /// Note that imggray should now be the difference image
	double Act_o = cv::mean(imggray, mask_o)[0];
	double dido = Act_i / Act_o;

	std::cout << "Inner = " << Act_i << " | Outer = " << Act_o << " | ratio: " << dido << endl;

	// Extra pause to allow display
	boost_sleep_ms(5000);
	watching_for_picked = NOT_WATCHING;
	return dido > 1.35;

}

// look for a worm in the frame that matches the picked worm
// assume: the worm will move away from agar indentations
// thus, we will be able to find a contour of roughly the same area after a brief
// period of time

// For 10s maintain vector of worm shapes that are potential matches
// STEP 1: Constantly look for new worms, matching the centroid on each frame to a previous frame
// STEP 2: if it's the same worm, flow track it to see if it's moving
// STEP 3: start flow tracking on next frame

bool Worms::wasWormDropped() {
	AnthonysTimer timeout;
	int movementThreshold = 50; // TODO - this needs to be calibrated
	double areaErr = 0.1; //look for contours within +/-10% of picked worm 
	vector<WormShape> potential_matches;

	while (timeout.getElapsedTime() < 10) {
		for (int i = 0; i < all_worms.size(); i++) {
			if (all_worms[i].getArea() < picked_worm.getArea()*(1 + areaErr) 
				|| picked_worm.getArea()*(1 - areaErr) < all_worms[i].getArea()) {
				// this contour is of the right size
				// if it isnt already tracked, at it to potential_matches
				if (isWormNew(all_worms[i], potential_matches, 20)) {
					potential_matches.push_back(all_worms[i]);
				}
			}
		}

		//check if any of the potential matches are moving
		for (int i = 0; i < potential_matches.size(); i++) {
			//update all the potential match contours
			try {
				/*
					WormShape one for each worm
					increase a time specifying how long it has been tracked for. Timer in constructor
						-->If can't update, go to catch and clear.
				*/

				//potential_matches[i].updateContour(flowPass(imggray, potential_matches[i]));
				//////////////////////////////////////////// TODO - DEPRECATED BY WORMSHAPE METHOD
			}
			catch (...) {
				potential_matches[i].clear();
			}
			

			// check each worm for its movement
			if (potential_matches[i].getDistanceTraveled() > movementThreshold) {
				//this is our worm. Set it as tracked so we can see the result
				findOnClick(potential_matches[i].getWormCentroid().x, potential_matches[i].getWormCentroid().y);
				std::cout << "DEBUG: Worm found after dropping" << endl;
				return true;
			}
		}
		boost_sleep_ms(100);
	}
	std::cout << "DEBUG: Worm NOT found after dropping" << endl;
	return false;
	
}

///******************************************************************* Used in quickPick methods inside Wormpick.cpp, but none of those methods are used.
double Worms::WormMvmFromPickedSite() {

	int monitor_time = 5;
	double total_len = 0;
	Point last_position = tracked_worm.getWormCentroid();
	Point current_position = Point (-1, -1);

	// Measure the total movement distance of picked worm right after picking
	for (int i = 0; i < monitor_time; i++) {

		boost::lock_guard<boost::mutex> guard(mtx_);

		boost_sleep_ms(1000);
		current_position = tracked_worm.getWormCentroid();

		if (last_position != Point(-1, -1) && last_position != Point(-1, -1)) {
			total_len += ptDist(last_position, current_position);
			last_position = current_position;
		}
		else {
			cout << "Worm lose tracked!" << endl;
			return total_len;
		}
	}

	return 0;
}


///******************************************************************* Used in WormPicker.cpp, but is commented out.
void Worms::updateTrack(bool wormSelectMode) {
	
	//if tracking is not enabled, do nothing
	if (!trackingEnabled) {
		return;
	}
	
	
	if (wormSelectMode) {
		roughPass();
	}
	else {
		extractBlobs();
	}

	evaluateBlobs(imggray);

	//if there is a tracked contour, try to track it with flow pass
	//if this fails, look for a new worm
	//if there isnt a tracked contour look for a new worm

	try {
		//tracked_worm.updateContour(flowPass(imggray, tracked_worm));
		//////////////////////////////////////////// TODO - DEPRECATED BY WORMSHAPE METHOD
	}
	catch (...) { // could not track the worm, look for a new one
		resetTrack();
	}

}

///******************************************************************* Used in quickPick methods inside Wormpick.cpp, but none of those methods are used. Used in ScriptActions.cpp but is commented out
bool Worms::findWormNearestPickupLoc() {

	// Lock the object during this method to prevent manipulation of the current objects while attempting
	// to find the best object (causes crashes)
	StrictLock<Worms> guard(*this);


	// Create a local copy of contours and centroids to use in this function to avoid thread conflicts
	/* Not needed with locking */

	/*
		Option 1: Re-find using flow contours (TODO / just clear for now)
	*/


	/*
		OPTION 2: Find the centroid whose distance to the previous one is minimized, unless tracking is at corner (no worm)
	*/
	resetTrack();
	AnthonysTimer Ta;
	double time_limit_s = 2;

	// Keep trying for up to 1 seconds
	while (tracked_worm.isEmpty() && Ta.getElapsedTime() < time_limit_s){

		double min_dist = 999;
		int min_idx = -1;

		// Find all the contours and compare to the pickup location
		for (int i = 0; i < all_worms.size(); i++) {
			double this_dist = ptDist(pickup_location, all_worms[i].getWormCentroid());
			if (this_dist < min_dist) {
				min_dist = this_dist;
				min_idx = i;
			}
		}

		// If any centroid was found to be within a certain distance, update this as the tracked centroid and start tracking
			/// If worm lost for a certain time, reset centroid too

		if (min_dist < track_radius*2) {
			tracked_worm.reset(all_worms[min_idx].getContour());
			Ta.setToZero(); /// Since we've found a worm, set the lost-worm timer to zero
			break;
		}

		boost::this_thread::sleep(boost::posix_time::milliseconds(10));

	}

	return !tracked_worm.isEmpty();
}

///******************************************************************* Used in quickPick methods inside Wormpick.cpp, and a method in OxCnc.cpp, but none of these are used.
bool Worms::findWormNearestPickupLocCNN() {

	// Lock the object during this method to prevent manipulation of the current objects while attempting
	// to find the best object (causes crashes)
	StrictLock<Worms> guard(*this);


	// Create a local copy of contours and centroids to use in this function to avoid thread conflicts
	/* Not needed with locking */

	/*
		Option 1: Re-find using flow contours (TODO / just clear for now)
	*/


	/*
		OPTION 2: Find the centroid whose distance to the previous one is minimized, unless tracking is at corner (no worm)
	*/
	resetTrack();
	AnthonysTimer Ta;
	double time_limit_s = 3;

	// Keep trying for up to 1 seconds
	while (tracked_worm.isEmpty_centroid() && Ta.getElapsedTime() < time_limit_s) {

		double min_dist = 999;
		int min_idx = -1;

		// Find all the contours and compare to the pickup location
		for (int i = 0; i < centroidAll.size(); i++) {
			double this_dist = ptDist(getPickupLocation(), centroidAll[i]);
			if (this_dist < min_dist) {
				min_dist = this_dist;
				min_idx = i;
			}
		}

		// If any centroid was found to be within a certain distance, update this as the tracked centroid and start tracking
			/// If worm lost for a certain time, reset centroid too

		if (min_dist < track_radius * 2) {
			tracked_worm.reset_centroid(centroidAll[min_idx]);
			Ta.setToZero(); /// Since we've found a worm, set the lost-worm timer to zero
			break;
		}

		boost::this_thread::sleep(boost::posix_time::milliseconds(10));

	}

	return !tracked_worm.isEmpty_centroid();
}


bool Worms::getWormNearestPickupLocCNN() {

	// Lock the object during this method to prevent manipulation of the current objects while attempting
	// to find the best object (causes crashes)
	StrictLock<Worms> guard(*this);


	// Create a local copy of contours and centroids to use in this function to avoid thread conflicts
	/* Not needed with locking */

	/*
		Option 1: Re-find using flow contours (TODO / just clear for now)
	*/


	/*
		OPTION 2: Find the centroid whose distance to the previous one is minimized, unless tracking is at corner (no worm)
	*/

	AnthonysTimer Ta;
	double time_limit_s = 15;

	// Keep trying for up to 15 seconds
	resetTrack();
	while (tracked_worm.isEmpty_centroid() && Ta.getElapsedTime() < time_limit_s) {

		if (!tracked_worm.isEmpty_centroid()) {
			break;
		}
		boost::this_thread::sleep(boost::posix_time::milliseconds(10));

	}

	return !tracked_worm.isEmpty_centroid();
}


double Worms::getDistToPickup() const {
	return ptDist(tracked_worm.getWormCentroid(), getPickupLocation());
}

Point Worms::getPickupLocation() const{
	return pickup_location;
}

///********************************************************** Used in executeCenterFluoWorm in TestScript.cpp, which is also declared (but never defined) in ScriptActions. Will ExecuteCenterFluoWorm end up being a ScriptAction?
cv::Point Worms::getCtrOvlpFov() const {
	return Point(round(ovlpFov.x + 0.5*ovlpFov.width), round(ovlpFov.y + 0.5*ovlpFov.height));
}

void Worms::setPickupLocation(int x, int y) {
	pickup_location.x = x;
	pickup_location.y = y;
	return;
}

///******************************************************************* Used in quickPick methods inside Wormpick.cpp, but none of those methods are used.
void Worms::setPickedWorm() {
	picked_worm.reset(tracked_worm.getContour());
	return;
}

///******************************************************************* Not used
void Worms::enableTracking() {
	trackingEnabled = true;
}

///******************************************************************* Not used
void Worms::disableTracking() {
	resetTrack();
	trackingEnabled = false;
}

///******************************************************************* Only used in TestScripts.cpp
WormShape Worms::getTrackedWorm() const{
	return tracked_worm;
}

///********************************************************** Used in executeCenterFluoWorm in TestScript.cpp, which is also declared (but never defined) in ScriptActions. Will ExecuteCenterFluoWorm end up being a ScriptAction?
WormShape Worms::getHiMagWorm() const {
	return hiMag_worm;
}

Point Worms::getTrackedCentroid() const{
	return tracked_worm.getWormCentroid();
}

Rect Worms::getTrackedRoi() const {
	return tracked_worm.getWormRoi();
}

///********************************************************** Not used
vector<Rect> Worms::getMonitorRoi() const {
	return Monitored_ROIs;
}

///********************************************************** Is commented out in display_images methods of WPHelper.cpp.
vector<Point> Worms::getTrackedContour() const {
	return tracked_worm.getContour();
}

vector<vector<Point>> Worms::getWormContours() const {
	vector<vector<Point>> result;
	for (int i = 0; i < all_worms.size(); i++) {
		result.push_back(all_worms[i].getContour());
	}
	return result;
}

vector<Point> Worms::getcentroidAll() const {
	return centroidAll;
}

std::vector<double> Worms::getareaAll() const {
	return areaAll;
}

///********************************************************** Only used in TestScript.cpp. Is commented out everywhere else.
std::vector<int> Worms::getPixelDiff() const {
	return pixel_change_rois;
}

///********************************************************** Used in unused quickpick methods in WormPicker.cpp. Also used in TestScripts.cpp
std::vector<double> Worms::getConfiSinglefrmCnn() const {
	return ConfiCentroid_singlefrmCnn;
}

cv::Point Worms::getcentroidbyidx(int idx) const {
	cv::Point this_point = centroidAll[idx];
	return Point((int)this_point.x, (int)this_point.y);
}

int Worms::getNumCentroid() const {
	return (int)centroidAll.size();
}

int Worms::getNumBlobs() const {
	return (int) all_worms.size();
}

bool Worms::isWormNew(const WormShape &worm, const vector<WormShape> &worm_list, int 
	_radius) {
	for (int i = 0; i < worm_list.size(); i++) {
		if (ptDist(worm.getWormCentroid(), worm_list[i].getWormCentroid()) < 20) {
			return false;
		}
		
	}
	return true;
}

bool Worms::isWormOnPickup() const{
	if (tracked_worm.isEmpty()) {
		return false;
	}

	if (getDistToPickup() <= max_dist_to_pickup) {
		return true;
	}
	else {
		return false;
	}
}

bool Worms::isWormOnPickupCNN() const {
	if (tracked_worm.isEmpty_centroid()) {
		return false;
	}

	if (getDistToPickup() <= max_dist_to_pickup) {
		return true;
	}
	else {
		return false;
	}
}



bool Worms::isWormTracked() const {
	return !tracked_worm.isEmpty();
}

///********************************************************** Only used in methods in this file, none of which are used
std::pair<cv::Point, int> Worms::CalCentroidEnergy(double alpha, double beta, double gamma) {

	double max_energy = -999;
	int max_idx = -1;

	// Compute the energy of all centroid
	vector<double> vec_energy;
	for (int i = 0; i < centroidAll.size(); i++) {
		double this_energy = alpha * DistCentroidtoPickupLoc[i] + beta * pixel_change_rois[i] + gamma * ConfiCentroid_singlefrmCnn[i];
		if (this_energy > max_energy) {
			max_energy = this_energy;
			max_idx = i;
		}

		vec_energy.push_back(this_energy);
		//if (findWorminWholeImg) {
			//cout << "Energy of Worm " << i << " = " << this_energy << "(Movement = " << pixel_change_rois[i] << ")" << endl;
		//}
	}
	UpdateEnergyCentroid(vec_energy);


	cout << max_idx << endl;
	// Assign the centroid with maximum energy
	if (max_energy != -999 && max_idx != -1) {
		//if (findWorminWholeImg) {
		//	cout << "Worm " << max_idx << " has max energy" << endl;
		//}
		max_energy_centroid = centroidAll[max_idx];
	}

	if (max_idx == -1) {
		max_energy_centroid = Point(-1, -1);
	}

	return make_pair(max_energy_centroid, max_idx);
}

///********************************************************** Not used
void Worms::setSearchRoi(cv::Point center, int radius) {
	// ROI
	search_roi.x = center.x - radius;
	search_roi.y = center.y - radius;
	search_roi.width = 2 * radius;
	search_roi.height = 2 * radius;

	// Prevent OOB
	search_roi.x = max(search_roi.x, 0);
	search_roi.y = max(search_roi.y, 0);
	search_roi.width = min(search_roi.width, imggray.size().width - search_roi.x);
	search_roi.height = min(search_roi.height, imggray.size().height - search_roi.y);
}

///********************************************************** Not used
void Worms::resetSearchRoi() {
	search_roi.x = 0;
	search_roi.y = 0;
	search_roi.width = imggray.size().width;
	search_roi.height = imggray.size().height;
}

///********************************************************** Only used in methods in this file, none of which are used
void Worms::GetHighEnergyCentroid(double ener_thresh) {

	vector<Point> centroid_high_energy(centroidAll);

	for (int i = 0; i < EnergyofCentroids.size(); i++) {
		if (EnergyofCentroids[i] < ener_thresh) {
			centroid_high_energy[i] = Point(-1, -1);
		}
	}
	centroid_high_energy.erase(remove(centroid_high_energy.begin(), centroid_high_energy.end(), Point(-1, -1)), centroid_high_energy.end());
	
	UpdateHighEnergyCentroids(centroid_high_energy);
}

///********************************************************** Not used
cv::Point2d Addoffset(cv::Point original_pt, cv::Point2d offset) {
	return cv::Point2d(original_pt.x + offset.x, original_pt.y + offset.y);
}

///********************************************************** Only used in methods in this file, none of which are used
void Worms::MonitorWormROIs( vector<Point>& centers, int roi_radius, cv::Mat& imggray, cv::Mat& DiffImg, cv::dnn::Net graynet,
									double dsfactor, double conf_thresh, int norm_type, double max_dist, double alpha, double beta, double gamma)
{
	
	//Test get stage coordinate
	//cout << "Coordinate = (" << Current_coord.x << " , " << Current_coord.y << ")" << endl;

	// Make local copy of image for resizing
	Mat image;
	imggray.copyTo(image);

	// Calculate pixel changes in this frame
	//vector<int> PixelDiff;

	// Update the pixel changes in ROI
	assignPixDiffRoi(RoiPixelDiff(DiffImg, centers));
	//PrintCentroidMovement();// print out movement

	// Calculate centroid for next frame;
	vector<Point> centroid_next_frame;
	vector<Rect> Rois;
	vector<double> prob_coords;

	// Compute the image offset since last position
	// Point2d offset((Current_coord.x - last_coordinate.x) * 36, (Current_coord.y - last_coordinate.y) * 36);
	// TO DO: get the dydox from Pk
	//UpdatePixOffset(Current_coord, last_coordinate, 36);

	for (int i = 0; i < centers.size(); i++) {

		// Set the ROI for each high energy centroid
		Rect ROI;
		ROI.x = centers[i].x - roi_radius;
		ROI.y = centers[i].y - roi_radius;
		ROI.width = 2 * roi_radius;
		ROI.height = 2 * roi_radius;

		// Prevent OOB
		ROI.x = max(ROI.x, 0);
		ROI.y = max(ROI.y, 0);
		ROI.width = min(ROI.width, image.size().width - ROI.x);
		ROI.height = min(ROI.height, image.size().height - ROI.y);

		Rois.push_back(ROI);

		// Crop the image in ROI
		Mat Roi_img;
		image(ROI).copyTo(Roi_img);

		// Dummy imgout, we don't need it
		Mat imgout;

		// Run CNN to get output
		CnnOutput out_roi = findWormsInImage(Roi_img, graynet, dsfactor, imgout, false, 0, 0, 0, norm_type);

		prob_coords.push_back(out_roi.max_prob);

		if (out_roi.max_prob > conf_thresh) {
			// Get the coordinate of worm with respect to the ROI image
			Point2d ctr_mass = CenterofMass(out_roi.coords, out_roi.probs) / dsfactor;
			Point2d pt_max_prob = out_roi.max_prob_point / dsfactor;

			Point2d posit_worm;

			if (trackbyCtrofMass) {
				posit_worm = ctr_mass;
			}
			else {
				posit_worm = pt_max_prob;
			}

			//if (ptDist(ctr_mass, pt_max_prob) > 10) {
			//	posit_worm = ctr_mass;
			//}
			//else {
			//	if (trackbyCtrofMass) {
			//		posit_worm = ctr_mass;
			//	}
			//	else {
			//		posit_worm = pt_max_prob;
			//	}
			//}

			// Convert the coordinate of worm respect to the whole image
			posit_worm.x = ROI.x + posit_worm.x;
			posit_worm.y = ROI.y + posit_worm.y;

			// Limit the maximum movement
			if (limit_max_dist) {
				if (ptDist(centers[i], posit_worm) < 10) {
					centroid_next_frame.push_back(posit_worm);
				}
				else {
					centroid_next_frame.push_back(centers[i]);
				}
			}
			else {
				centroid_next_frame.push_back(posit_worm);
			}
		}
		// If the highest probability is less than a threshold value do not update the centroid
		else {
			centroid_next_frame.push_back(centers[i]);
		}

	}

	// Update coordinate
	//last_coordinate = Current_coord;

	UpdateMonitorRois(Rois);
	UpdateHighEnergyCentroids(centroid_next_frame);
	assigncentroidAll(centroid_next_frame);

	// Update confidence value of centroid
	UpdateConfiSinglefrmCnn(prob_coords);

	// Update centroids distance to pick up location
	UpdateCentroidDist();

	// Calculate the energy functional and set the centroid with highest energy to track
	pair<Point, int> p = CalCentroidEnergy(alpha, beta, gamma);

	cout << p.second << endl;
	if (p.second == -1) {
		resetTrack();
		cout << "No high energy centroids found, press w to refind" << endl;
		boost_sleep_ms(500);
	}
	else {
		tracked_worm.reset_centroid(p.first);
		tracked_worm.reset_roi(Rois[p.second]);

		// Only track the worm that has maximum energy in the next frame
		if (!trackwormNearPcikupLoc && centers.size() > 1) {
			UpdateMonitorRois({ Rois[p.second] });
			UpdateHighEnergyCentroids({ p.first });
			assigncentroidAll({ p.first });
			UpdateConfiSinglefrmCnn({ prob_coords[p.second] });
			UpdateCentroidDist();
		}

	}
}

///********************************************************** Only used in methods in this file, none of which are used
void Worms::MonitorWormROIs(const ControlPanel CtrlPanel)
{

	//Test get stage coordinate
	//cout << "Coordinate = (" << Current_coord.x << " , " << Current_coord.y << ")" << endl;

	// Make local copy of image for resizing
	Mat image;
	imggray.copyTo(image);

	// Calculate pixel changes in this frame
	//vector<int> PixelDiff;

	// Update the pixel changes in ROI
	assignPixDiffRoi(RoiPixelDiff(Diff_img, HighEnergyCentroids));
	//PrintCentroidMovement();// print out movement

	// Calculate centroid for next frame;
	vector<Point> centroid_next_frame;
	vector<Rect> Rois;
	vector<double> prob_coords;

	// Compute the image offset since last position
	// Point2d offset((Current_coord.x - last_coordinate.x) * 36, (Current_coord.y - last_coordinate.y) * 36);
	// TO DO: get the dydox from Pk
	//UpdatePixOffset(Current_coord, last_coordinate, 36);

	for (int i = 0; i < HighEnergyCentroids.size(); i++) {

		// Set the ROI for each high energy centroid
		Rect ROI;
		ROI.x = HighEnergyCentroids[i].x - CtrlPanel.rad_roi;
		ROI.y = HighEnergyCentroids[i].y - CtrlPanel.rad_roi;
		ROI.width = 2 * CtrlPanel.rad_roi;
		ROI.height = 2 * CtrlPanel.rad_roi;

		// Prevent OOB
		ROI.x = max(ROI.x, 0);
		ROI.y = max(ROI.y, 0);
		ROI.width = min(ROI.width, image.size().width - ROI.x);
		ROI.height = min(ROI.height, image.size().height - ROI.y);

		Rois.push_back(ROI);

		// Crop the image in ROI
		Mat Roi_img;
		image(ROI).copyTo(Roi_img);

		// Dummy imgout, we don't need it
		Mat imgout;

		// Run CNN to get output
		CnnOutput out_roi = findWormsInImage(Roi_img, CtrlPanel.net, CtrlPanel.dsfactor, imgout, false, 0, 0, 0, CtrlPanel.norm_type);

		prob_coords.push_back(out_roi.max_prob);

		if (out_roi.max_prob > CtrlPanel.conf_thresh) {
			// Get the coordinate of worm with respect to the ROI image
			Point2d ctr_mass = CenterofMass(out_roi.coords, out_roi.probs);
			Point2d pt_max_prob = out_roi.max_prob_point;

			Point2d posit_worm;

			if (trackbyCtrofMass) {
				posit_worm = ctr_mass;
			}
			else {
				posit_worm = pt_max_prob;
			}

			//if (ptDist(ctr_mass, pt_max_prob) > 10) {
			//	posit_worm = ctr_mass;
			//}
			//else {
			//	if (trackbyCtrofMass) {
			//		posit_worm = ctr_mass;
			//	}
			//	else {
			//		posit_worm = pt_max_prob;
			//	}
			//}

			// Convert the coordinate of worm respect to the whole image
			posit_worm.x = ROI.x + posit_worm.x;
			posit_worm.y = ROI.y + posit_worm.y;

			// Limit the maximum movement
			if (limit_max_dist) {
				if (ptDist(HighEnergyCentroids[i], posit_worm) < 10) {
					centroid_next_frame.push_back(posit_worm);
				}
				else {
					centroid_next_frame.push_back(HighEnergyCentroids[i]);
				}
			}
			else {
				centroid_next_frame.push_back(posit_worm);
			}
		}
		// If the highest probability is less than a threshold value do not update the centroid
		else {
			centroid_next_frame.push_back(HighEnergyCentroids[i]);
		}

	}

	// Update coordinate
	//last_coordinate = Current_coord;

	UpdateMonitorRois(Rois);
	UpdateHighEnergyCentroids(centroid_next_frame);
	assigncentroidAll(centroid_next_frame);

	// Update confidence value of centroid
	UpdateConfiSinglefrmCnn(prob_coords);

	// Update centroids distance to pick up location
	UpdateCentroidDist();

	// Calculate the energy functional and set the centroid with highest energy to track
	pair<Point, int> p = CalCentroidEnergy(CtrlPanel.alpha, CtrlPanel.beta, CtrlPanel.gamma);

	if (p.second == -1) {
		resetTrack();
		cout << "No high energy centroids found, press w to refind" << endl;
		boost_sleep_ms(500);
	}
	else {
		tracked_worm.reset_centroid(p.first);
		tracked_worm.reset_roi(Rois[p.second]);

		// Only track the worm that has maximum energy in the next frame
		if (!trackwormNearPcikupLoc && HighEnergyCentroids.size() > 1) {
			UpdateMonitorRois({ Rois[p.second] });
			UpdateHighEnergyCentroids({ p.first });
			assigncentroidAll({ p.first });
			UpdateConfiSinglefrmCnn({ prob_coords[p.second] });
			UpdateCentroidDist();
		}

	}
}


///********************************************************** Not used
void Worms::MonitorWormPickupLoc(int roi_radius, cv::Mat& imggray, cv::Mat& DiffImg, cv::dnn::Net graynet, double dsfactor, double conf_thresh, int norm_type, double alpha, double beta, double gamma) {

	// Make local copy of image for resizing
	Mat image;
	imggray.copyTo(image);

	// Calculate pixel changes in this frame
	//vector<int> PixelDiff;

	// Update the pixel changes in ROI
	assignPixDiffRoi(RoiPixelDiff(DiffImg, { getPickupLocation() }));

	// Calculate centroid for next frame;
	Point centroid_next_frame;
	double prob_coord;

	// Set the ROI near the pick-up loc
	Rect ROI;

	ROI.x = getPickupLocation().x - roi_radius;
	ROI.y = getPickupLocation().y - roi_radius;
	ROI.width = 2 * roi_radius;
	ROI.height = 2 * roi_radius;

	// Prevent OOB
	ROI.x = max(ROI.x, 0);
	ROI.y = max(ROI.y, 0);
	ROI.width = min(ROI.width, image.size().width - ROI.x);
	ROI.height = min(ROI.height, image.size().height - ROI.y);

	// Crop the image in ROI
	Mat Roi_img;
	image(ROI).copyTo(Roi_img);

	// Dummy imgout, we don't need it
	Mat imgout;

	// Run CNN to get output
	CnnOutput out_roi = findWormsInImage(Roi_img, graynet, dsfactor, imgout, false, 0, 0, 5, norm_type);

	prob_coord = out_roi.max_prob;

	// Get the coordinate of worm with respect to the ROI image
	//Point posit_worm = out_roi.max_prob_point / dsfactor;
	Point2d posit_worm = CenterofMass(out_roi.coords, out_roi.probs) / dsfactor;
	// Convert the coordinate of worm respect to the whole image
	posit_worm.x = ROI.x + posit_worm.x;
	posit_worm.y = ROI.y + posit_worm.y;
	centroid_next_frame = posit_worm;


	UpdateMonitorRois({ ROI });
	UpdateHighEnergyCentroids({ centroid_next_frame });
	assigncentroidAll({ centroid_next_frame });

	// Update confidence value of centroid
	UpdateConfiSinglefrmCnn({ prob_coord });

	// Update centroids distance to pick up location
	UpdateCentroidDist();

	// Calculate the energy functional and set the centroid with highest energy to track
	pair<Point, int> p = CalCentroidEnergy(alpha, beta, gamma);

	if (p.second == -1) {
		resetTrack();
		cout << "No high energy centroids found, press w to refind" << endl;
		boost_sleep_ms(500);
	}
	else {
		tracked_worm.reset_centroid(p.first);
		tracked_worm.reset_roi(ROI);
	}

}




///********************************************************** Only used in methods in this file, none of which are used
cv::Point2d GausWgtSum(std::vector<cv::Point> vec_pts, std::vector<double> vec_probs, double sigma) {
	
	// Define the dimension of kernel
	int sq_sz = vec_pts.size();
	std::vector<double> weights;
	std::vector<double> weighted_probs;
	int sz = sqrt(sq_sz);
	double Pi = 3.14159;

	double r, s = 2 * sigma * sigma;
	double sum = 0; // sum for normalization
	// Require odd number
	if (sz % 2 == 1) {
		for (int x = -(sz - 1) / 2; x <= (sz - 1) / 2; x++) {
			for (int y = -(sz - 1) / 2; y <= (sz - 1) / 2; y++) {
				r = sqrt(x * x + y * y);
				weights.push_back((exp(-(r * r) / s)) / (Pi * s));
			}
		}

		// Calculate sum
		sum = totalmass(weights);
		
		// Normalize the Kernel
		for (int i = 0; i < weights.size(); i++) {
			weights[i] /= sum;
		}

		// Compute weighted mass

		if (weights.size() == vec_probs.size()) {
			for (int j = 0; j < weights.size(); j++) {
				weighted_probs.push_back(weights[j] * vec_probs[j]);
			}

			return CenterofMass(vec_pts, weighted_probs);
		}
		else { return Point2d(-1, -1); }

	}
	else {
		cout << "Fail to compute Gaussian weighted sum: # of kernel size should be odd!" << endl;
		return Point2d(-1, -1);
	}
}


///********************************************************** Only used in methods in this file, none of which are used
void Worms::MonitorWormRoiGaus(std::vector<cv::Point>& centers, int roi_radius, cv::Mat& imggray, cv::Mat& DiffImg, cv::dnn::Net graynet, double dsfactor, double conf_thresh, int norm_type, double max_dist, double alpha, double beta, double gamma) {
	//Test get stage coordinate
	//cout << "Coordinate = (" << Current_coord.x << " , " << Current_coord.y << ")" << endl;

	// Make local copy of image for resizing
	Mat image;
	imggray.copyTo(image);

	// Calculate pixel changes in this frame
	//vector<int> PixelDiff;

	// Update the pixel changes in ROI
	assignPixDiffRoi(RoiPixelDiff(DiffImg, centers));
	//PrintCentroidMovement();// print out movement

	// Calculate centroid for next frame;
	vector<Point> centroid_next_frame;
	vector<Rect> Rois;
	vector<double> prob_coords;

	// Compute the image offset since last position
	// Point2d offset((Current_coord.x - last_coordinate.x) * 36, (Current_coord.y - last_coordinate.y) * 36);
	// TO DO: get the dydox from Pk
	//UpdatePixOffset(Current_coord, last_coordinate, 36);

	for (int i = 0; i < centers.size(); i++) {

		// Set the ROI for each high energy centroid
		Rect ROI;
		ROI.x = centers[i].x - roi_radius;
		ROI.y = centers[i].y - roi_radius;
		ROI.width = 2 * roi_radius;
		ROI.height = 2 * roi_radius;

		// Prevent OOB
		ROI.x = max(ROI.x, 0);
		ROI.y = max(ROI.y, 0);
		ROI.width = min(ROI.width, image.size().width - ROI.x);
		ROI.height = min(ROI.height, image.size().height - ROI.y);

		Rois.push_back(ROI);

		// Crop the image in ROI
		Mat Roi_img;
		image(ROI).copyTo(Roi_img);

		// Dummy imgout, we don't need it
		Mat imgout;

		// Run CNN to get output
		CnnOutput out_roi = findWormsInImage(Roi_img, graynet, dsfactor, imgout, false, 0, 0, 0, norm_type);

		prob_coords.push_back(out_roi.max_prob);

		if (out_roi.max_prob > conf_thresh) {
			// Get the coordinate of worm with respect to the ROI image
			//Point2d ctr_mass = CenterofMass(out_roi.coords, out_roi.probs) / dsfactor;
			Point2d ctr_mass = GausWgtSum(out_roi.coords, out_roi.probs, 2) / dsfactor;
			Point2d pt_max_prob = out_roi.max_prob_point / dsfactor;

			Point2d posit_worm;

			if (trackbyCtrofMass) {
				posit_worm = ctr_mass;
			}
			else {
				posit_worm = pt_max_prob;
			}

			//if (ptDist(ctr_mass, pt_max_prob) > 10) {
			//	posit_worm = ctr_mass;
			//}
			//else {
			//	if (trackbyCtrofMass) {
			//		posit_worm = ctr_mass;
			//	}
			//	else {
			//		posit_worm = pt_max_prob;
			//	}
			//}

			// Convert the coordinate of worm respect to the whole image
			posit_worm.x = ROI.x + posit_worm.x;
			posit_worm.y = ROI.y + posit_worm.y;

			// Limit the maximum movement
			if (limit_max_dist) {
				if (ptDist(centers[i], posit_worm) < 10) {
					centroid_next_frame.push_back(posit_worm);
				}
				else {
					centroid_next_frame.push_back(centers[i]);
				}
			}
			else {
				centroid_next_frame.push_back(posit_worm);
			}
		}
		// If the highest probability is less than a threshold value do not update the centroid
		else {
			centroid_next_frame.push_back(centers[i]);
		}

	}

	// Update coordinate
	//last_coordinate = Current_coord;

	UpdateMonitorRois(Rois);
	UpdateHighEnergyCentroids(centroid_next_frame);
	assigncentroidAll(centroid_next_frame);

	// Update confidence value of centroid
	UpdateConfiSinglefrmCnn(prob_coords);

	// Update centroids distance to pick up location
	UpdateCentroidDist();

	// Calculate the energy functional and set the centroid with highest energy to track
	pair<Point, int> p = CalCentroidEnergy(alpha, beta, gamma);

	if (p.second == -1) {
		resetTrack();
		cout << "No high energy centroids found, press w to refind" << endl;
		boost_sleep_ms(500);
	}
	else {
		tracked_worm.reset_centroid(p.first);
		tracked_worm.reset_roi(Rois[p.second]);

		// Only track the worm that has maximum energy in the next frame
		if (!trackwormNearPcikupLoc && centers.size() > 1) {
			UpdateMonitorRois({ Rois[p.second] });
			UpdateHighEnergyCentroids({ p.first });
			assigncentroidAll({ p.first });
			UpdateConfiSinglefrmCnn({ prob_coords[p.second] });
			UpdateCentroidDist();
		}

	}



}

///********************************************************** Not used
void Worms::SearchOrMonitorWorms(const ControlPanel CtrlPanel) {
	Monitored_ROIs.clear();
	Mat image;
	imggray.copyTo(image);
	Mat Roi_img;
	CnnOutput out_roi;
	imggray(CtrlPanel.search_ROI).copyTo(Roi_img);
	// Dummy imgout, we don't need it
	Mat imgout;
	vector<Point> centroid_next_frame;
	vector<Rect> Rois;
	vector<double> prob_coords;
	vector<Point> worm_coords;


	if (CtrlPanel.track_worm_option == SEARCH) {
		out_roi = findWormsInImage(Roi_img, CtrlPanel.net, CtrlPanel.dsfactor, imgout, false, 0, 0, CtrlPanel.conf_thresh, CtrlPanel.norm_type);
		for (int i = 0; i < size(out_roi.classes); i++)
		{
			if (out_roi.classes[i] == 0) { // Class 0 represents worm
				Point coord_Roi = out_roi.coords[i] / (double)CtrlPanel.dsfactor;
				Point coord_whole_img = Point(CtrlPanel.search_ROI.x + coord_Roi.x, CtrlPanel.search_ROI.y + coord_Roi.y);
				worm_coords.push_back(coord_whole_img);
				prob_coords.push_back(out_roi.probs[i]);
			}
		}
		// Points closed to each other are removed, represented by a point that has the highest probability among them
		RemoveCloseWorms(worm_coords, prob_coords, 20);

		// Assign the coords to worms
		assigncentroidAll(worm_coords);

		// Update confidence value of centroid
		UpdateConfiSinglefrmCnn(prob_coords);
		// Update centroids distance to pick up location
		UpdateCentroidDist();

		// Update the pixel changes in ROI
		assignPixDiffRoi(RoiPixelDiff(Diff_img, centroidAll));

		// Calculate the energy functional
		CalCentroidEnergy(CtrlPanel.alpha, CtrlPanel.beta, CtrlPanel.gamma);

		// Remove the centroid having energy lower than threshold
		GetHighEnergyCentroid(CtrlPanel.energy_thresh);


	}
	if (CtrlPanel.track_worm_option == MONITOR) {
		// Update the pixel changes in ROI
		assignPixDiffRoi(RoiPixelDiff(Diff_img, HighEnergyCentroids));
		//PrintCentroidMovement();// print out movement

		// Calculate centroid for next frame;
		for (int i = 0; i < HighEnergyCentroids.size(); i++) {

		// Set the ROI for each high energy centroid
		Rect ROI;
		ROI.x = HighEnergyCentroids[i].x - CtrlPanel.rad_roi;
		ROI.y = HighEnergyCentroids[i].y - CtrlPanel.rad_roi;
		ROI.width = 2 * CtrlPanel.rad_roi;
		ROI.height = 2 * CtrlPanel.rad_roi;

		// Prevent OOB
		ROI.x = max(ROI.x, 0);
		ROI.y = max(ROI.y, 0);
		ROI.width = min(ROI.width, image.size().width - ROI.x);
		ROI.height = min(ROI.height, image.size().height - ROI.y);

		Rois.push_back(ROI);

		// Crop the image in ROI
		Mat Roi_img;
		image(ROI).copyTo(Roi_img);

		// Dummy imgout, we don't need it
		Mat imgout;

		// Run CNN to get output
		out_roi = findWormsInImage(Roi_img, CtrlPanel.net, CtrlPanel.dsfactor, imgout, false, 0, 0, 0, CtrlPanel.norm_type);

		prob_coords.push_back(out_roi.max_prob);

		if (out_roi.max_prob > CtrlPanel.conf_thresh) {
			// Get the coordinate of worm with respect to the ROI image
			Point2d ctr_mass = CenterofMass(out_roi.coords, out_roi.probs);
			Point2d pt_max_prob = out_roi.max_prob_point;

			Point2d posit_worm;

			if (trackbyCtrofMass) {
				posit_worm = ctr_mass;
			}
			else {
				posit_worm = pt_max_prob;
			}

				//if (ptDist(ctr_mass, pt_max_prob) > 10) {
				//	posit_worm = ctr_mass;
				//}
				//else {
				//	if (trackbyCtrofMass) {
				//		posit_worm = ctr_mass;
				//	}
				//	else {
				//		posit_worm = pt_max_prob;
				//	}
				//}

				// Convert the coordinate of worm respect to the whole image
				posit_worm.x = ROI.x + posit_worm.x;
				posit_worm.y = ROI.y + posit_worm.y;

				// Limit the maximum movement
				if (limit_max_dist) {
					if (ptDist(HighEnergyCentroids[i], posit_worm) < 10) {
						centroid_next_frame.push_back(posit_worm);
					}
					else {
						centroid_next_frame.push_back(HighEnergyCentroids[i]);
					}
				}
				else {
					centroid_next_frame.push_back(posit_worm);
				}
			}
			// If the highest probability is less than a threshold value do not update the centroid
			else {
				centroid_next_frame.push_back(HighEnergyCentroids[i]);
			}

		}
		// Calculate pixel changes in this frame
		// vector<int> PixelDiff;
		UpdateMonitorRois(Rois);
		UpdateHighEnergyCentroids(centroid_next_frame);
		assigncentroidAll(centroid_next_frame);

		// Update confidence value of centroid
		UpdateConfiSinglefrmCnn(prob_coords);

		// Update centroids distance to pick up location
		UpdateCentroidDist();

		// Calculate the energy functional and set the centroid with highest energy to track
		pair<Point, int> p = CalCentroidEnergy(CtrlPanel.alpha, CtrlPanel.beta, CtrlPanel.gamma);

		if (p.second == -1) {
			resetTrack();
			cout << "No high energy centroids found, press w to refind" << endl;
			boost_sleep_ms(500);
		}
		else {
			tracked_worm.reset_centroid(p.first);
			tracked_worm.reset_roi(Rois[p.second]);

			// Only track the worm that has maximum energy in the next frame
			if (!trackwormNearPcikupLoc && HighEnergyCentroids.size() > 1) {
				UpdateMonitorRois({ Rois[p.second] });
				UpdateHighEnergyCentroids({ p.first });
				assigncentroidAll({ p.first });
				UpdateConfiSinglefrmCnn({ prob_coords[p.second] });
				UpdateCentroidDist();
			}

		}


	}


	// Run CNN to get output
	//Test get stage coordinate
//cout << "Coordinate = (" << Current_coord.x << " , " << Current_coord.y << ")" << endl;
// Make local copy of image for resizing


}

///********************************************************** Only used in methods in this file, none of which are used
void Worms::PrintCentroidMovement() const{
	if (pixel_change_rois.size() != 0) {
		for (int i = 0; pixel_change_rois.size(); i++) {
			cout << "Movement of Worm " << i << ": " << pixel_change_rois[i] << endl;
		}
	}
	else {
		//cout << "No worm was identified: toggle w to refind the worm" << endl;
	}
}


void Worms::assigncentroidAll(std::vector<cv::Point> coords) {
	centroidAll.clear();
	for (int i = 0; i < coords.size(); i++)
		centroidAll.push_back(coords[i]);
}

///********************************************************** Only used in methods in this file, none of which are used
void Worms::assignPixDiffRoi(std::vector<int> vec_PixDiff) {
	pixel_change_rois.clear();
	for (int i = 0; i < vec_PixDiff.size(); i++) {
		pixel_change_rois.push_back(vec_PixDiff[i]);
	}
}

///********************************************************** Only used in methods in this file, none of which are used
void Worms::UpdateEnergyCentroid(std::vector<double> vec_energy) {
	EnergyofCentroids.clear();
	for (int i = 0; i < vec_energy.size(); i++) {
		EnergyofCentroids.push_back(vec_energy[i]);
	}
}

///********************************************************** Only used in methods in this file, none of which are used
void Worms::UpdateCentroidDist() {
	DistCentroidtoPickupLoc.clear();
	// Find all the centroids and compare to the pickup location
	for (int i = 0; i < centroidAll.size(); i++) {
		DistCentroidtoPickupLoc.push_back(ptDist(getPickupLocation(), centroidAll[i]));
	}
}

///********************************************************** Not used.... also... empty
void Worms::UpdateMvDist() {

}

///********************************************************** Only used in methods in this file, none of which are used
void Worms::UpdateConfiSinglefrmCnn(vector<double> vec_conf) {
	ConfiCentroid_singlefrmCnn.clear();
	for (int i = 0; i < vec_conf.size(); i++) {
		ConfiCentroid_singlefrmCnn.push_back(vec_conf[i]);
	}
}

///********************************************************** Only used in methods in this file, none of which are used
void Worms::UpdateHighEnergyCentroids(std::vector<cv::Point> vec_centroid) {
	HighEnergyCentroids. clear();
	for (int i = 0; i < vec_centroid.size(); i++) {
		HighEnergyCentroids.push_back(vec_centroid[i]);
	}
}

void Worms::UpdateMonitorRois(std::vector<cv::Rect> vec_rois) {
	Monitored_ROIs.clear();
	for (int i = 0; i < vec_rois.size(); i++) {
		Monitored_ROIs.push_back(vec_rois[i]);
	}
}

///********************************************************** Only used in unused methods in this file and is also commented out everywhere
void Worms::UpdatePixOffset(cv::Point2d pt1, cv::Point2d pt2, double dydOx) {
	offset = Point2d((pt1.x - pt2.x) * dydOx, (pt2.y - pt2.y) * dydOx);
}

///********************************************************** Only used in methods in this file, none of which are used
void Worms::UpdatecontoursAll(std::vector<std::vector<cv::Point>>& cturs) {

	contoursAll.clear();

	for (int i = 0; i < cturs.size(); i++) {
		contoursAll.push_back(cturs[i]);
	}
	
}

///********************************************************** Only used in methods in this file, none of which are used
void Worms::UpdateAreaAll() {

	areaAll.clear();
	
	for (int i = 0; i < contoursAll.size(); i++) {
		areaAll.push_back(contourArea(contoursAll[i]));
	}

}

///********************************************************** Only used in methods in this file, none of which are used
void Worms::ChooseOneWorm(ControlPanel CtrlPanel) {
	if (centroidAll.size() == 0) {
		return;
	}
	vector<Point> centroid_high_energy;
	centroid_high_energy.push_back(centroidAll[0]);


	for (int i = 0; i < EnergyofCentroids.size() - 1; i++) {
		if (EnergyofCentroids[i] < EnergyofCentroids[i + 1]) {
			centroid_high_energy[0] = centroidAll[i+1];
		}
		
	}
	UpdateHighEnergyCentroids(centroid_high_energy);

}



///********************************************************** Not used
// Make p1's location index smaller than p2
void pair_wrapper(pair<int, double>& p1, pair<int, double>& p2) {
	if (p1.first > p2.first) {
		pair<int, double> p3;
		p3 = p1;
		p1 = p2;
		p2 = p3;
	}
}


/*
	AssessHeadTail is script for testing head and tail disambiguation methods
	Due to recent reorganization it will not work as written because most functions
	Have now been moved to WormShape::segmentWorm(). If you want to use  it again
	you can uncomment below and create a WormShape object to represent your worm for testing. 
*/

//void Worms::AssessHeadTail(const ControlPanel& CtrlPanel, const cv::Mat& cur_frm, const cv::Mat& pre_frm, std::pair<double, double>& angles, std::pair<double, double>& movement, std::pair<double, double>& intensity, vector<double>& body_inten, double length, double width) {
//
//	// 1st: threshold image to get the candidate head and tail
//	// Get the thresholded images
//	// Dummy inputs, we dont need it
//	vector<Point> worm_pos;
//	vector<double> worm_ergy;
//	vector<Mat> pix_diff;
//
//	// Get the thresholded current frame
//	Mat nor_cur_frm;
//	normalize(cur_frm, nor_cur_frm, 0, 255, NORM_MINMAX);
//	vector<Mat> vec_thres_cur_frm;
//	ParalWormFinder(nor_cur_frm, pre_frame_hi_mag, CtrlPanel, worm_pos, worm_ergy, pix_diff, vec_thres_cur_frm);
//	Mat thres_cur_frm;
//	vec_thres_cur_frm[0].copyTo(thres_cur_frm);
//
//
//	// Get the thresholded previous frame
//	Mat nor_pre_frm;
//	normalize(pre_frm, nor_pre_frm, 0, 255, NORM_MINMAX);
//	vector<Mat> vec_thres_pre_frm;
//	ParalWormFinder(nor_pre_frm, pre_frame_hi_mag, CtrlPanel, worm_pos, worm_ergy, pix_diff, vec_thres_pre_frm);
//	Mat thres_pre_frm;
//	vec_thres_pre_frm[0].copyTo(thres_pre_frm);
//
//	// Get the worm contour
//	isolateLargestBlob(thres_cur_frm, 0);
//	isolateLargestBlob(thres_pre_frm, 0);
//
//	vector<vector<Point>> contoursTemp_cur;
//	vector<Vec4i> hierarchy_cur;
//	findContours(thres_cur_frm, contoursTemp_cur, hierarchy_cur, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
//
//	vector<vector<Point>> contoursTemp_pre;
//	vector<Vec4i> hierarchy_pre;
//	findContours(thres_pre_frm, contoursTemp_pre, hierarchy_pre, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
//	
//	if (!contoursTemp_cur.empty() && !contoursTemp_pre.empty()) {
//
//		Mat color_cur_frm;
//		cvtColor(nor_cur_frm, color_cur_frm, COLOR_GRAY2RGB);
//		Mat color_pre_frm;
//		cvtColor(nor_pre_frm, color_pre_frm, COLOR_GRAY2RGB);
//
//		// 2nd: Measure the contour angle of head and tail
//		// Get the location and contour angle of candidate head and tail
//		pair<int, double> loc_ang_pr1, loc_ang_pr2;
//		getCandidateHeadTail(contoursTemp_cur[0], loc_ang_pr1, loc_ang_pr2, 0.3);
//		pair_wrapper(loc_ang_pr1, loc_ang_pr2);
//
//
//		namedWindow("nor_cur_frm", WINDOW_AUTOSIZE);
//		imshow("nor_cur_frm", nor_cur_frm);
//		waitKey(1);
//
//		pair<int, double> pre_frm_pr1, pre_frm_pr2;
//		getCandidateHeadTail(contoursTemp_pre[0], pre_frm_pr1, pre_frm_pr2, 0.3);
//		pair_wrapper(pre_frm_pr1, pre_frm_pr2);
//
//		angles.first = loc_ang_pr1.second;
//		angles.second = loc_ang_pr2.second;
//		cout << "Angles: "  << angles.first  << ", " << angles.second << endl;
//
//
//		circle(color_cur_frm, contoursTemp_cur[0][loc_ang_pr1.first], 15, colors::rr, 1);
//		circle(color_cur_frm, contoursTemp_cur[0][loc_ang_pr2.first], 15, colors::bb, 1);
//		//namedWindow("Cur frame 1", WINDOW_AUTOSIZE);
//		//imshow("Cur frame 1", color_cur_frm);
//		//waitKey(1);
//
//
//		circle(color_pre_frm, contoursTemp_pre[0][pre_frm_pr1.first], 15, colors::rr, 1);
//		circle(color_pre_frm, contoursTemp_pre[0][pre_frm_pr2.first], 15, colors::bb, 1);
//		/*namedWindow("Pre frame 1", WINDOW_AUTOSIZE);
//		imshow("Pre frame 1", color_pre_frm);
//		waitKey(1);*/
//
//
//		// 3rd: Measure the head and tail intensity
//		// Get the dorsal and ventral boundary
//		vector<Point> DrlBdry, VtrlBdry;
//		splitContour(contoursTemp_cur[0], loc_ang_pr1.first, loc_ang_pr2.first, DrlBdry, VtrlBdry);
//
//		vector<Point> DrlBdry_pre, VtrlBdry_pre;
//		splitContour(contoursTemp_pre[0], pre_frm_pr1.first, pre_frm_pr2.first, DrlBdry_pre, VtrlBdry_pre);
//	
//		// Resample the dorsal and ventral boundaries into multiple bins along the body
//		vector<Point> resmpl_DrlBdry, resmpl_VtrlBdry;
//		getResampledBdry(DrlBdry, resmpl_DrlBdry, 100);
//		getResampledBdry(VtrlBdry, resmpl_VtrlBdry, 100);
//
//		vector<Point> resmpl_DrlBdry_pre, resmpl_VtrlBdry_pre;
//		getResampledBdry(DrlBdry_pre, resmpl_DrlBdry_pre, 100);
//		getResampledBdry(VtrlBdry_pre, resmpl_VtrlBdry_pre, 100);
//
//		if (resmpl_DrlBdry.size() <= 1 || resmpl_VtrlBdry.size() <= 1 || resmpl_DrlBdry_pre.size() <= 1 || resmpl_VtrlBdry_pre.size() <= 1) {
//			cout << "Fail to resample curve!" << endl;
//			return void();
//		}
//
//		polylines(color_cur_frm, resmpl_DrlBdry, false, colors::yy, 1);
//		polylines(color_cur_frm, resmpl_VtrlBdry, false, colors::gg, 1);
//		/*namedWindow("Cur frame 2", WINDOW_AUTOSIZE);
//		imshow("Cur frame 2", color_cur_frm);
//		waitKey(1);*/
//
//
//		polylines(color_pre_frm, resmpl_DrlBdry_pre, false, colors::yy, 1);
//		polylines(color_pre_frm, resmpl_VtrlBdry_pre, false, colors::gg, 1);
//		/*namedWindow("Pre frame 2", WINDOW_AUTOSIZE);
//		imshow("Pre frame 2", color_pre_frm);
//		waitKey(1);*/
//
//		// Get the centerline
//		vector<Point> ctrline_cur, ctrline_pre;
//		getCenterline(ctrline_cur, resmpl_DrlBdry, resmpl_VtrlBdry);
//		getCenterline(ctrline_pre, resmpl_DrlBdry_pre, resmpl_VtrlBdry_pre);
//
//		polylines(color_cur_frm, ctrline_cur, false, colors::pp, 1);
//
//
//		polylines(color_pre_frm, ctrline_pre, false, colors::pp, 1);
//
//		
//
//		// Get the region masks of head and tail
//		cv::Size imgsz = Size(cur_frm.cols, cur_frm.rows);
//		Mat head_mask, tail_mask;
//		getRegionMask(head_mask, imgsz, resmpl_DrlBdry, resmpl_VtrlBdry, 0, length, width);
//		getRegionMask(tail_mask, imgsz, resmpl_DrlBdry, resmpl_VtrlBdry, (1-length), 1, width);
//
//		// Get the coordinated mask along the body
//		vector<Mat> masks;
//		getCoordMask(masks, imgsz, resmpl_DrlBdry, resmpl_VtrlBdry, width);
//
//		getBinMeanInten(body_inten, nor_cur_frm, masks);
//
//		for (int i = 0; i < body_inten.size(); i++) {
//			cout << body_inten [i] << endl;
//		}
//
//		// Calculate the mean intensity for head and tail region
//		vector<double> head_tail_inten;
//		getBinMeanInten(head_tail_inten, cur_frm, { head_mask, tail_mask });
//
//		vector<vector<Point>> head_ctrs; vector<Vec4i> head_hiera;
//		findContours(head_mask, head_ctrs, head_hiera, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
//		drawContours(color_cur_frm, head_ctrs, -1, colors::rr);
//
//		vector<vector<Point>> tail_ctrs; vector<Vec4i> tail_hiera;
//		findContours(tail_mask, tail_ctrs, tail_hiera, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
//		drawContours(color_cur_frm, tail_ctrs, -1, colors::bb);
//
//		namedWindow("Cur frame 3", WINDOW_AUTOSIZE);
//		imshow("Cur frame 3", color_cur_frm);
//		waitKey(1);
//
//		namedWindow("Pre frame 3", WINDOW_AUTOSIZE);
//		imshow("Pre frame 3", color_pre_frm);
//		waitKey(1);
//
//
//		intensity.first = head_tail_inten[0];
//		intensity.second = head_tail_inten.back();
//
//		cout << "Intensity: " << intensity.first << ", " << intensity.second << endl;
//
//		// 4th: Measure the movement of head and tail in the past 2s
//		// Get the skeleton movement at the head and tail
//		double head_mvt = 0, tail_mvt = 0;
//		vector<Point> head_ske_1, head_ske_2;
//		getSkeletonMovement(ctrline_cur, ctrline_pre, head_mvt, 0, length, head_ske_1, head_ske_2);
//		vector<Point> tail_ske_1, tail_ske_2;
//		getSkeletonMovement(ctrline_cur, ctrline_pre, tail_mvt, (1-length), 1, tail_ske_1, tail_ske_2);
//
//
//		Mat head_ske_img = Mat::zeros(imgsz, CV_8UC3);
//		polylines(head_ske_img, head_ske_1, false, colors::rr, 5);
//		polylines(head_ske_img, head_ske_2, false, colors::rr, 5);
//		namedWindow("Head skeleton", WINDOW_AUTOSIZE);
//		imshow("Head skeleton", head_ske_img);
//		waitKey(1);
//
//		Mat tail_ske_img = Mat::zeros(imgsz, CV_8UC3);
//		polylines(tail_ske_img, tail_ske_1, false, colors::bb, 5);
//		polylines(tail_ske_img, tail_ske_2, false, colors::bb, 5);
//		namedWindow("Tail skeleton", WINDOW_AUTOSIZE);
//		imshow("Tail skeleton", tail_ske_img);
//		waitKey(1);
//
//		// Skeleton movement
//		Mat ske_img = Mat::zeros(imgsz, CV_8UC3);
//		polylines(ske_img, resmpl_DrlBdry, false, colors::yy, 1);
//		polylines(ske_img, resmpl_VtrlBdry, false, colors::gg, 1);
//		polylines(ske_img, ctrline_cur, false, colors::pp, 1);
//
//		polylines(ske_img, resmpl_DrlBdry_pre, false, colors::yy, 1);
//		polylines(ske_img, resmpl_VtrlBdry_pre, false, colors::gg, 1);
//		polylines(ske_img, ctrline_pre, false, colors::pp, 1);
//
//		polylines(ske_img, head_ske_1, false, colors::rr, 2);
//		polylines(ske_img, head_ske_2, false, colors::rr, 2);
//		polylines(ske_img, tail_ske_1, false, colors::bb, 2);
//		polylines(ske_img, tail_ske_2, false, colors::bb, 2);
//
//		namedWindow("Skeleton movement", WINDOW_AUTOSIZE);
//		imshow("Skeleton movement", ske_img);
//		waitKey(1);
//
//
//		movement.first = head_mvt;
//		movement.second = tail_mvt;
//
//		cout << "Movement: " << movement.first << ", " << movement.second << endl;
//
//	}
//
//}


bool Worms::WrmCrwlOutOfPk() {


	// The 1st version of unloading verifier
	// Find there is a worm found near the unloading position
	// Toggle the worm finding mode

	//worm_finding_mode = 2;

	//boost_sleep_ms(4000);

	//worm_finding_mode = 1;

	//boost_sleep_ms(2000);

	//return tracked_worm.isEmpty();


	// The 2nd version of unloading verifier
	// Change to finding mode
	worm_finding_mode = 0;

	// Record a few seconds of frames, for trajectory analysis

	cout << "Recording start" << endl;
	vector<Mat> travel_history;
	for (int i = 0; i < 30; i++) {
		// Mat debug; imggray.copyTo(debug);
		//namedWindow("Display frame", CV_WINDOW_AUTOSIZE);
		//imshow("Display frame", debug);
		//waitKey(1);
		travel_history.push_back(imggray);
		boost_sleep_ms(100); // Sampling rate
	}



	// Change to tracking mode, tracking the worm nearest to the unloading site
	// worm_finding_mode = 1;

	// Trajectory analysis: back-tracing the traveling path of this worm in the frame history
	vector<vector<Point>> Trajs_of_frms = WormTrajectory(travel_history, centroidAll);

	// Switch the axis of vectors
	vector<vector<Point>> Trajs_of_worms;
	SwapAxis(Trajs_of_frms, Trajs_of_worms);

	// To check whether these is a worm crawl out from unloading site.
	bool wrm_crwl_out = false;

	if (!Trajs_of_worms.empty()) {

		origin_traj.clear();
		for (int i = 0; i < Trajs_of_worms.size(); i++) {

			origin_traj.push_back(Trajs_of_worms[i].back());
			double DisToPk = ptDist(origin_traj[i], pickup_location);
			cout << "Worm " << i << "crawling from point x = " << origin_traj[i].x << ", y = " << origin_traj[i].y;
			cout << "Dist. to unloading pt = " << DisToPk << endl;
			if (DisToPk < 300) {
				wrm_crwl_out = true;
			}
		}
	}
	else {
		cout << "Worm not found!" << endl;
		wrm_crwl_out = false;
	}

	return wrm_crwl_out;
}


///********************************************************** Not used
std::vector<double> Worms::getCurvatures(vector<Point> input) const {
	vector<double> ret;
	int x = 7;
	for (int i = 2*x; i < input.size() + 2*x; i++) {
		Point three = input[i % input.size()];
		Point two = input[(i - x) % input.size()];
		Point one = input[(i - 2*x) % input.size()];
		Point2f der1, der2;
		der1.x = (three.x - two.x) / 2*x;
		der1.y = (three.y - two.y) / 2*x;
		der2.x = (- three.x + 2 * two.x - one.x) / x / x;
		der2.y = (- three.y + 2 * two.y - one.y) / x / x;
		double num = (der2.y*der1.x - der2.x*der1.y)*(der2.y*der1.x - der2.x*der1.y);
		double denom = (der2.y + der2.x);
		denom = denom * denom * denom;
		cout << num << " " << denom << " " << endl;
		ret.push_back(sqrt(num / denom));
	}
	rotate(ret.begin(), ret.end() - 2, ret.end());
	return ret;
}

