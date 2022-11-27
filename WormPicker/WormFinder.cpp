#include "WormFinder.h"
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
#include "mask_rcnn.h"
#include "ScriptActions.h"



using namespace cv;
using namespace std;
using json = nlohmann::json;

// Constructor with default values for all thresholds

WormFinder::WormFinder(Size sz, int& wat, std::string fonnx_worm, Mat& full_res_bw_img, Mat& high_mag_img, Mat& img_high_mag_color, Mat& img_low_mag_color) :
	full_img_size(sz),
	watching_for_picked(wat),
	fonnx_worm(fonnx_worm),
	imggray(full_res_bw_img),
	img_high_mag(high_mag_img),
	img_high_mag_color(img_high_mag_color),
	img_low_mag_color(img_low_mag_color)
{
	worm_finding_mode = WAIT;
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

	Size sz_low_color = img_low_mag_color.size();
	Size full_img_size = imggray.size();

	json pickup_config = Config::getInstance().getConfigTree("pickup");
	pickup_location.x = pickup_config["location"]["x"].get<int>();
	pickup_location.y = pickup_config["location"]["y"].get<int>();
	high_mag_fov_center_loc = pickup_location;
	// pickup_location = Point(-1, -1);
	//pickup_location = Point(320, 242); original pickup location in old optics
	//pickup_location = Point(1294, 938);
	// pickup_location = Point(287, 200);

	//high_mag_fov_center_loc = Point(1294, 938); // The x and y values used here are approximated and will need to be exacted later

	loc_to_move_user_set = Point(double(sz_low_color.width) / 2, double(sz_low_color.height) / 2);

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
	last_coordinate = Point2d(0, 0);

	// ROI of pick-up location
	Point pickup_pt = pickup_location;
	pickup_Roi.x = pickup_pt.x - pickup_radius;
	pickup_Roi.y = pickup_pt.y - pickup_radius;
	pickup_Roi.width = 2 * pickup_radius;
	pickup_Roi.height = 2 * pickup_radius;
	//verify_roi = Rect(-1, -1, 1, 1);

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
	ControlPanel ConPan(sz, fonnx_worm);
	ctrlPanel = ConPan;

	// Set the search region for verifying the unloading success
	unload_search_Roi.x = -1;
	unload_search_Roi.y = -1;
	unload_search_Roi.width = 0;
	unload_search_Roi.height = 0;
	// High magnification CNN is a static member of Worm and set in Worm.cpp

	// Define the pickable region

	double h_multiplier = double(sz_low_color.height) / double(full_img_size.height);
	double w_multiplier = double(sz_low_color.width) / double(full_img_size.width);

	/*cout << "################# h_mult: " << h_multiplier << endl;
	cout << "################# W_mult: " << w_multiplier << endl;*/

	pickable_region.radius = imggray.size().height * 0.35; //0.28
	pickable_region.original_center = Point((imggray.size().width / 2) + 45, imggray.size().height / 2);
	pickable_region.pickable_center = Point((imggray.size().width / 2) + 45, imggray.size().height / 2);
	//pickable_region = Rect (imggray.size().width * 0.3 + 150, imggray.size().height * 0.25 + 50, imggray.size().width * 0.3, imggray.size().height * 0.5);
	

	// Define verify radius over whole plate (radius for search dropped worm over the entire plate)
	verify_radius_over_whole_plate = imggray.size().width / 2 - 50;

    // Set offset for laser autofocusing
    json focus_config = Config::getInstance().getConfigTree("imaging_mode")["LASER_AUTOFOCUS"];
    laser_focus_low_mag_offset = focus_config["Low_mag_offset"].get<int>();
    laser_focus_high_mag_offset = focus_config["High_mag_offset"].get<int>();
}

// Destructor

WormFinder::~WormFinder()
{
}

// Load parameters from disk. If bad parameters are detected, throw error
void WormFinder::loadParameters() {

	Config::getInstance().loadFiles();
	json config = Config::getInstance().getConfigTree("selection criteria");

	thresh = config["thresh"].get<int>();   // Change threshold to be confidence threshold (0.7 default)
	contrast_max = config["contrast max"].get<int>(); //deprecateD?
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

void WormFinder::writeParameters() {
	if (!isWormTracked()) {
		return;
	}
	Config::getInstance().loadFiles();
	json config = Config::getInstance().getConfigTree("selection criteria");

	double a = tracked_worm->getArea();
	double p = arcLength(tracked_worm->getContour(), true);
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

//rough pass that replaces extract blobs when in worm select mode
//all viable contours are outlined
int WormFinder::roughPass() {
	int cnt = 0;

	bwcontours.setTo(0);
	all_worms.clear();

	//if the contour is trivially big, add it contours
	for (int i = 0; i < contoursAll.size(); i++) {
		if (contoursAll[i].size() > 18) {
			cnt++;
			all_worms.push_back(Worm(contoursAll[i]));
		}
	}

	return cnt;
}

int WormFinder::extractBlobs() {
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
		if (iskeep && (ptDist(getCentroid(contoursAll[i]), tracked_worm->getWormCentroid()) < 20 && isWormTracked())) {
			iskeep = false;
		}

		if (iskeep) {
			// Increment the extracted blob counter
			ct++;

			// If the blob matches the thresholds, draw it on the binary image
			drawContours(bwcontours, contoursAll, i, colors::ww, -1, 8);

			// If the blob matches the thresholds, add it to the extracted contous list
			all_worms.push_back(Worm(contoursAll[i]));
		}
	}

	// Return the number of blobs
	return ct;
}


void WormFinder::evaluateBlobs(const cv::Mat &frame) {

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


double WormFinder::totalmass(std::vector<double> vec_mass) {
	double sum_mass = 0;
	for (int i = 0; i < vec_mass.size(); i++) {
		sum_mass += vec_mass[i];
	}
	return sum_mass;
}


cv::Point2d WormFinder::CenterofMass(std::vector<cv::Point> vec_pts, std::vector<double> vec_probs) {

	double ctr_x = -1; double ctr_y = -1;

	if ((vec_pts.size() == vec_probs.size()) && vec_pts.size() > 0) {

		double weighted_sum_x = 0;
		double weighted_sum_y = 0;
		for (int i = 0; i < vec_pts.size(); i++) {

			weighted_sum_x += vec_probs[i] * double(vec_pts[i].x);
			weighted_sum_y += vec_probs[i] * double(vec_pts[i].y);
		}

		if (totalmass(vec_probs) != 0) {

			double center_x = weighted_sum_x / totalmass(vec_probs);
			double center_y = weighted_sum_y / totalmass(vec_probs);

			ctr_x = center_x;
			ctr_y = center_y;
		}
		else {
			// cout << "Fail to calculate center of mass: sum of mass is 0" << endl;
		}

	}
	else {
		cout << "Fail to calculate center of mass: empty vector or unequal vector size" << endl;
	}

	return Point2d(ctr_x, ctr_y);
}

// Calculate the coordinate of each bin
void WormFinder::CalBinCoor(vector<Point>& ds_coords, vector<Point>& coords, Size sz, double dsfactor) {
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
//void WormFinder::RemoveClosePts(vector<Point>& worm_coords, vector<double>& prob_coords, double dist) {
void WormFinder::RemoveCloseWorms(double dist, vector<Worm>& removed_worms) {

	for (int i = 0; i < size(all_worms); i++) {
		for (int j = i + 1; j < size(all_worms); j++) {
			Worm& worm_one = all_worms[i];
			Worm& worm_two = all_worms[j];
			double this_dist = ptDist(worm_one.getWormCentroidGlobal(), worm_two.getWormCentroidGlobal());
			if (this_dist < dist) {
				if (worm_one.getWormEnergy() > worm_two.getWormEnergy()) {
					worm_two.setEnergy(-999);
				}
				else {
					worm_one.setEnergy(-999);
				}
			}
		}
	}

	int remove_count = 0;

	// Remove the worms we set to have -999 energy
	for (int i = all_worms.size() - 1; i >= 0; i--) {
		if (all_worms[i].getWormEnergy() == -999) {
			remove_count++;
			removed_worms.push_back(all_worms[i]);
			all_worms.erase(all_worms.begin() + i);
		}
	}
	//cout << "Removed " << remove_count << " worms due to closeness." << endl;
}

// This method checks the area around the pick after a picking or a dropping action has been performed in order
// to verify the success of that action by finding the presence or absence or a worm, respectively.
bool WormFinder::wormPickVerification() {

	// First we need to gather motion data around the picking site
	image_timer.setToZero();
	worm_finding_mode = FIND_ALL;
	return false;

}

void WormFinder::swapWormFindingMode(int mode, std::vector<cv::Rect>& finding_rois) {
	worm_finding_mode = mode;
	ctrlPanel.ROIs = finding_rois;
}

void WormFinder::runWormFinder() {
	worm_finding_mode = WAIT;
	ControlPanel& control_panel = ctrlPanel;


	int loop_num = 0;
	double cur_img_to_pre_img_delay = 3; //1.6 // Number of seconds seperating the current image and pre-image we send to the Parallel Worm Finder
	int loop_delay = 10; // Milliseconds delay between loops
	int grab_image_delay = 200; // Milliseconds between adding images to image queue, should evenly divide the cur_img_to_pre_img_delay value (when in ms).
	int next_img_save_index = 0;

	int num_images_to_save = cur_img_to_pre_img_delay * 1000 / grab_image_delay;

	vector<double> image_times;

	for (int i = 0; i < num_images_to_save; i++) {
		Mat img;
		image_queue.push_back(img);
		image_times.push_back(0);
	}

	//AnthonysTimer image_timer;
	AnthonysTimer grab_image_timer;
	image_timer.setToZero();
	while (true) {
		//cout << "grab timer time: " << grab_image_timer.getElapsedTime() << endl;
		if (grab_image_timer.getElapsedTime() >= double(grab_image_delay) / 1000) {
			//if (loop_num % (grab_image_delay / loop_delay) == 0) {

			imggray.copyTo(image_queue[next_img_save_index]);
			image_times[next_img_save_index] = image_timer.getElapsedTime();
			//cout << "replacing image at index: " << next_img_save_index << endl;
			//cout << next_img_save_index << ", " << num_images_to_save << endl;

			if (next_img_save_index == num_images_to_save - 1) {
				next_img_save_index = 0;
			}
			else {
				next_img_save_index++;
			}

			grab_image_timer.setToZero();
		}


		if (worm_finding_mode != WAIT) {
			// Before we run the finder we must ensure that enough time has passed that the image vector is full.
			// TODO : if we move to a new plate we'll need to reset the timer so the image vector can refill with images of the new plate.
			if (image_timer.getElapsedTime() <= cur_img_to_pre_img_delay + double(grab_image_delay) / 1000) {
				//cout << "waiting to gather images for wormfinder: " << image_timer.getElapsedTime() << endl;
				continue;
			}

			most_recent_img_idx = 0;
			if (next_img_save_index == 0) {
				most_recent_img_idx = num_images_to_save - 1;
			}
			else {
				most_recent_img_idx = next_img_save_index - 1;
			}
			//int curr_idx = next_img_save_index == 0 ? num_images_to_save - 1 : next_img_save_index - 1;

			//Mat& curr_img = image_queue[curr_idx];
			//Mat& prev_img = image_queue[next_img_save_index];
			//cout << "Current time: " << image_timer.getElapsedTime() << endl;
			//cout << "Image times: " << image_times[curr_idx] << ", " << image_times[next_img_save_index] << endl;
			/*for (double a : image_times) {
				cout << a << ", ";
			}
			cout << endl;*/

			prepControlPanel(control_panel);



			//// get gray image
			//Mat img0;
			//imggray.copyTo(img0);

			//cout << "waiting before taking next next image" << endl;
			//// Wait 2 seconds (to allow for movement)
			//boost_sleep_ms(1000 * 2);
			//cout << "Done waiting." << endl;
			//// Get second (current) gray image
			//Mat img1;
			//imggray.copyTo(img1);

			/*imshow("img0", img0);
			waitKey(100);
			imshow("img1", img1);
			waitKey(100);*/

			//ParalWormFinder(img1, img0, control_panel);
			ParalWormFinder(most_recent_img_idx, next_img_save_index, image_queue, control_panel);
			if (worm_finding_mode == FIND_ALL) { 
				worm_finding_mode = WAIT; 
				control_panel.custom_finding_rois.clear();
			}
		}

		boost_sleep_ms(loop_delay);
		boost_interrupt_point();
		//loop_num++;

	}
}

//void WormFinder::ParalWormFinder(const cv::Mat& cur_frm, const cv::Mat& pre_frm, const ControlPanel CtrlPanel, std::vector<cv::Point>& worm_posit, std::vector<double>& vec_ergy,
//	std::vector<cv::Mat>& diff_img, std::vector<cv::Mat>& thres_img)
void WormFinder::ParalWormFinder(int cur_frm_idx, int pre_frm_idx, const vector<Mat>& images, const ControlPanel CtrlPanel)
{
	AnthonysTimer timeFunc;
	vector<Mat> diff_img, thres_img;
	vector<Mat> hotspot_imgs; // Holds ROI images of movement hotspots to feed to the CNN

	//Grab the current frame and pre frame from the Wormfinder's image queue
	const Mat& cur_frm = images[cur_frm_idx];
	

	//cout << "========= Paral start. ROI size: " << CtrlPanel.ROIs.size() << endl;
	// Iterate through ROI images
	for (int i = 0; i < CtrlPanel.ROIs.size(); i++) {

		hotspot_imgs.clear();

		// Get Global coordinate, width and height of ROI
		int this_roi_x = CtrlPanel.ROIs[i].x;
		int this_roi_y = CtrlPanel.ROIs[i].y;
		int this_roi_width = CtrlPanel.ROIs[i].width;
		int this_roi_height = CtrlPanel.ROIs[i].height;

		// Get the ROI image
		Mat Roi_img, display_worm_locations_img;
		cur_frm(CtrlPanel.ROIs[i]).copyTo(Roi_img);
		cur_frm(CtrlPanel.ROIs[i]).copyTo(display_worm_locations_img);


		// Divide to bins
		// Declare vectors that hold coordinates of each bin
		// Network designed for 20x20 pixels image
		// Thus, Coordinates of bin center are fixed once a ROI is defined (size of bin can vary)
		vector<Point> ds_coords, coords;
		CalBinCoor(ds_coords, coords, Roi_img.size(), CtrlPanel.dsfactor);

		vector<Point> movement_hotspots; // Will contain a subset of coords that have high movement energy

		// Declare vectors that holds different energy components of each bin
		vector<double> total_ergy, mvm_ergy, conf_ergy, inten_ergy, mvm_energy_hotspots;
		for (int j = 0; j < ds_coords.size(); j++) {
			
			mvm_ergy.push_back(0);
			
			if (CtrlPanel.beta == 0) {
				// If Beta is 0 then we are not generating movement hotspots with a difference image, which means that the energy vectors will be the same size
				// as the coordinate vectors and we will have a value for every coordinate rather than just the hotspots. So we initalize the energy vectors here.
				total_ergy.push_back(0);
				conf_ergy.push_back(0);
				inten_ergy.push_back(0);
				mvm_energy_hotspots.push_back(0);
			}

		}

		// Processing Selector
		// Energy = alpha * Distance to pickup Loc + beta * (pixel change - mean) / SD + gamma * (Confidence - mean) / SD + delta * Intensity classification score
		if (CtrlPanel.beta != 0) {
			bool good_pre_frame = false;
			vector<int> vec_pix;

			while(!good_pre_frame){
				const Mat& pre_frm = images[pre_frm_idx];
				// Compute difference image
				Mat Roi_pre_img;
				pre_frm(CtrlPanel.ROIs[i]).copyTo(Roi_pre_img);
				Mat Pixel_change;

				//absdiff(Roi_img, Roi_pre_img, Pixel_change);

				Pixel_change = Roi_pre_img - Roi_img; // Subtract the current image from the previous image. This way we are only left with where the worms travel TO.
				// TODO: change to reg subtraction

				Mat small_pix_change;
				resize(Pixel_change, small_pix_change, Size(0, 0), 0.5, 0.5);
				/*imshow("Pixel Change 1", small_pix_change);
				waitKey(0);*/


				if (CtrlPanel.thres_diff_img > 0) { threshold(Pixel_change, Pixel_change, CtrlPanel.thres_diff_img, 255, THRESH_BINARY); }

				Mat small_pix_change2;
				resize(Pixel_change, small_pix_change2, Size(0, 0), 0.5, 0.5);
				/*imshow("Pixel Change", small_pix_change2);
				waitKey(1);*/
				//destroyWindow("Pixel Change 2");

				// Save diff image and threshold image to disk
				/*vector<int> params{ CV_IMWRITE_PNG_COMPRESSION, 0 };
				string fout_frames_diff = "C:\\WormPicker\\MotionData\\MotionImages\\";
				fout_frames_diff.append(getCurrentTimeString(false, true));
				fout_frames_diff.append("_DiffImage"); fout_frames_diff.append(to_string(pre_frm_idx)); fout_frames_diff.append(".png");
				imwrite(fout_frames_diff, small_pix_change, params);

				string fout_frames_thresh = "C:\\WormPicker\\MotionData\\MotionImages\\";
				fout_frames_thresh.append(getCurrentTimeString(false, true));
				fout_frames_thresh.append("_ThreshImage"); fout_frames_thresh.append(to_string(pre_frm_idx)); fout_frames_thresh.append(".png");
				imwrite(fout_frames_thresh, small_pix_change2, params);*/

				// Copy to output ROI difference image vector
				diff_img.push_back(Pixel_change);

				//// Open the motion data file to record the motion info
				//string tFile = "C:\\WormPicker\\MotionData\\pix_diff_";
				//string date_string = getCurrentDateString();

				//tFile.append(date_string); tFile.append(".txt");


				//motion_data.open(tFile, std::fstream::app);

				// Check the result to see if we got a bad diff image (too many bright spots)
				double num_bright_pixels = cv::sum(Pixel_change)[0]/255;

				cout << "Number of bright pixels: " << num_bright_pixels << endl;
				cout << "Percent of bright pixels: " << (num_bright_pixels / (Pixel_change.size().width * Pixel_change.size().height)) * 100 << "%" << endl;

				// If more than 0.2% of the overall image is bright then we assume we got a bad diff image
				double max_percent = 0.002;
				if (num_bright_pixels > Pixel_change.size().width * Pixel_change.size().height * max_percent) {
					cout << "****************** Bad pre image for subtraction **********************" << endl;
					// Bad diff image so move to the next pre image in the image queue and try again
					if (pre_frm_idx == images.size() - 1) {
						pre_frm_idx = 0;
					}
					else {
						pre_frm_idx++;
					}
					//motion_data.terminate();
					continue;
				}

				good_pre_frame = true;


				// Feed to motion detector
				vec_pix = measureBinPixDiff(coords, Pixel_change, CtrlPanel.rad_bin_motion, CtrlPanel.get_bin_mot);

			} 
			// Add the motion energy component
			for (int m = 0; m < vec_pix.size(); m++) {
				mvm_ergy[m] = CtrlPanel.beta * ((vec_pix[m] - CtrlPanel.miu_pix) / CtrlPanel.sd_pix);
			}

			// Check the movement energy of each bin against the motion threshold - If it passes the threshold then we create an ROI around that coordinate.
			// This way we can speed up the CNN by only feeding it the ROIs of these high movement hot spots. We do this rather than running the CNN on the entire image which is slow.
			for (int i = 0; i < mvm_ergy.size(); i++) {
				if (mvm_ergy[i] >= CtrlPanel.ergy_thres_motion) {
					/*int small_img_x = coords[i].x / 2.88;
					int small_img_y = coords[i].y / 3.24;
					cout << "Movement hotspot found: " << coords[i] << ", " << Point(small_img_x, small_img_y) << endl;*/
					movement_hotspots.push_back(coords[i]);
					mvm_energy_hotspots.push_back(mvm_ergy[i]);

					//Initialize other energy component vectors - the actual values will be filled in later, but the size should be the same as movement hotspots
					total_ergy.push_back(0);
					conf_ergy.push_back(0);
					inten_ergy.push_back(0);

				}
			}

			vector<Rect> hotspot_rois;
			getROIsfromPts(movement_hotspots, Roi_img.size(), hotspot_rois, 10);

			// Create mats from the ROIs to feed to the CNN
			for (Rect roi : hotspot_rois) {
				Mat hotspot_img;
				Roi_img(roi).copyTo(hotspot_img);
				hotspot_imgs.push_back(hotspot_img);
			}
		}
		
		if (movement_hotspots.size() == 0) {
			if (worm_finding_mode == FIND_ALL) {
				cout << "No movement hot spots found in find all mode. No worms will be found." << endl;
				all_worms.clear();
				tracked_worm = nullptr;
				//motion_data.terminate();
				return; 
			}
			movement_hotspots = coords;
			generate20x20Images(Roi_img, hotspot_imgs, CtrlPanel.dsfactor);
		}

		if (CtrlPanel.gamma != 0) {
			// Feed to CNN
			//CnnOutput out_roi = measureBinConf(ds_coords, Roi_img, CtrlPanel.net, CtrlPanel.dsfactor, CtrlPanel.rad_bin_cnn);
			//CnnOutput out_roi;
			//for (Mat hotspot_img : hotspot_imgs) {
			//	Mat img_out; // Shouldn't get used because out_img_mode defaults to 0
			//	CnnOutput out_roi_partial = findWormsInImage(hotspot_img, CtrlPanel.net, CtrlPanel.dsfactor, img_out, false);
			//	//out = classifyImages(images, net, conf_thresh, false, display_class_idx, norm_type);

			//	out_roi.probs.insert(out_roi.probs.end(), out_roi_partial.probs.begin(), out_roi_partial.probs.end());
			//}
			
			// Fill out default worm finding values for classify images
			double conf_thresh = 0.95;
			int display_class_idx = 0;
			int norm_type = 1;
			cv::dnn::Net classifyNet = CtrlPanel.net; // Must copy the net because ParallelWormFinder CtrlPanel is const, but Classify images modifies net (we don't need those modifications though)
			
			CnnOutput out_roi = classifyImages(hotspot_imgs, classifyNet, conf_thresh, false, display_class_idx, norm_type);

			//Mat img_out; // Shouldn't get used because out_img_mode defaults to 0
			//CnnOutput out_roi = findWormsInImage(Roi_img, CtrlPanel.net, CtrlPanel.dsfactor, img_out, false);

			//motion_data << "Confidence levels: " << endl;

			// Add the CNN energy component
			for (int n = 0; n < out_roi.probs.size(); n++) {
				conf_ergy[n] = CtrlPanel.gamma * ((out_roi.probs[n] - CtrlPanel.miu_conf) / CtrlPanel.sd_conf);

				//motion_data << out_roi.probs[i] << endl;
			}

			//motion_data.terminate();
		}

		if (CtrlPanel.delta != 0) {
			// Feed to Intensity classifier (binary etc.)
			vector<double> vec_inten_class = measureIntenBinClass(movement_hotspots, Roi_img, CtrlPanel.rad_bin_thres, CtrlPanel.inten_thresh, thres_img, CtrlPanel.blursize, CtrlPanel.smallsizetoremove, CtrlPanel.get_bin_class);

			// Add the intensity classification score
			for (int k = 0; k < vec_inten_class.size(); k++) {
				inten_ergy[k] = CtrlPanel.delta * vec_inten_class[k];
			}
		}

		// Calculate the total energy and maximum energy
		double max_energy = -99999;
		int max_idx = -1;
		for (int i = 0; i < total_ergy.size(); i++) {
			total_ergy[i] = mvm_energy_hotspots[i] + conf_ergy[i] + inten_ergy[i];

			if (total_ergy[i] > max_energy) {
				max_energy = total_ergy[i];
				max_idx = i;
			}
		}


		// Representation
		//ControlPanel::Repres_mode Rep_mode = (ControlPanel::Repres_mode)(CtrlPanel.Rep_idx);
		//if (Rep_mode == ControlPanel::MULT) {
		if (worm_finding_mode == FIND_ALL) {
			cout << "Finding all worms." << endl;
			all_worms.clear();
			tracked_worm = nullptr;
			// Multiple representation
			// Show all the bins that have energy value higher than a threshold
			vector<Point> rejected_as_worms;
			// Get high energy bin
			for (int i = 0; i < total_ergy.size(); i++) {
				if (total_ergy[i] > CtrlPanel.energy_thresh
					//&& mvm_ergy[i] >= CtrlPanel.ergy_thres_motion // No longer need to check mvm energy here because the procedure has been modified to only use movement hotspots.
					//&& conf_ergy[i] >= CtrlPanel.ergy_thres_cnn
					//&& inten_ergy[i] >= CtrlPanel.ergy_thres_inten
					) // Energy thresholder
				{

					// Convert the global coordinate of bin respect to the whole image 
					Point gbl_bin;
					/*gbl_bin.x = this_roi_x + coords[i].x;
					gbl_bin.y = this_roi_y + coords[i].y;*/
					gbl_bin.x = this_roi_x + movement_hotspots[i].x;
					gbl_bin.y = this_roi_y + movement_hotspots[i].y;

					// Append to worm position and total energy vector
					Worm worm = Worm();
					/*worm.setCentroid(coords[i]);
					worm.setCentroidGlobal(gbl_bin);*/
					worm.setCentroid(movement_hotspots[i]);
					worm.setGlbCentroidAndRoi(gbl_bin, CtrlPanel.rad_roi, full_img_size);
					worm.setEnergy(total_ergy[i]);
					//worm.setRoiFromCentroidGlb(CtrlPanel.rad_roi, full_img_size);
					all_worms.push_back(worm);
					//worm_posit.push_back(gbl_bin);
					//vec_ergy.push_back(total_ergy[i]);

					
				}
				else {
					Point gbl_bin;
					/*gbl_bin.x = this_roi_x + coords[i].x;
					gbl_bin.y = this_roi_y + coords[i].y;*/
					gbl_bin.x = this_roi_x + movement_hotspots[i].x;
					gbl_bin.y = this_roi_y + movement_hotspots[i].y;
					rejected_as_worms.push_back(gbl_bin);
				}
			}

			//cout << "Rejected " << rejected_as_worms.size() << " worms." << endl;

			vector<Worm> removed_worms;
			// Remove points that are too terminate to eachother
			RemoveCloseWorms(40, removed_worms);

			// Draw circles around all the worms we found/rejected.
			// Green are confirmed worms. Yellow are worms that were rejected due to closeness. Red are potential worms that were rejected due to total energy.
			drawAllWormPositions(display_worm_locations_img, colors::gg, false);
			drawCirclesAtPoints(display_worm_locations_img, rejected_as_worms, colors::rr);

			/*vector<Point> removed_worm_locs;
			for (Worm& w : removed_worms) {
				removed_worm_locs.push_back(w.getWormCentroidGlobal());
			}

			drawCirclesAtPoints(display_worm_locations_img, removed_worm_locs, colors::yy);*/

			//showDebugImage(display_worm_locations_img, false, "Worm Positions", 0);


		}
		//else if (Rep_mode == ControlPanel::SIN) {
		else if (worm_finding_mode == TRACK_ONE || worm_finding_mode == TRACK_ALL) {
			// Single representation

			// Find the center of mass of energy value of bins in ROI
			Point2d ctr_mass = CenterofMass(movement_hotspots, total_ergy);

			// Convert to global coordinate
			Point gbl_ctr_mass;
			gbl_ctr_mass.x = this_roi_x + ctr_mass.x;
			gbl_ctr_mass.y = this_roi_y + ctr_mass.y;

			//// Append to worm position vector
			//if (total_ergy[max_idx] > CtrlPanel.ergy_thres_mon) { worm_posit.push_back(gbl_ctr_mass); }
			//// If the maximum energy of this Roi cannot reach a threshold, then append the center of this Roi
			//else { worm_posit.push_back(Point(this_roi_x + this_roi_width / 2, this_roi_y + this_roi_height / 2)); }

			// Update worm position
			Worm& worm = all_worms[i];

			// Only update the tracked worm in TRACK_ONE mode
			if (worm_finding_mode == TRACK_ONE) { worm = *tracked_worm; }
			// only all the worms in track_all mode
			if (max_idx != -1 && total_ergy[max_idx] > CtrlPanel.ergy_thres_mon) {
				/*cout << "Mem addresses: " << &(all_worms[0]) << ", " << &worm << endl;
				cout << "Updating worm pos: " << i << ", " << worm.getWormCentroidGlobal() << ", " << gbl_ctr_mass << endl;*/
				updateWormPosition(worm, gbl_ctr_mass, total_ergy[max_idx], CtrlPanel.rad_roi, false);
			}
			else {
				updateWormPosition(worm, gbl_ctr_mass, total_ergy[max_idx], CtrlPanel.rad_roi, true);
			}

			
			//Worm worm = Worm();
			//Worm& worm_to_modify = worm;

			//// If the tracked worms coords are within the current ROI then we can assume this is the same worm and update the tracked worm
			//if (pointIsWithinRoi(tracked_worm->getWormCentroidGlobal(), CtrlPanel.ROIs[i])) {
			//	worm_to_modify = tracked_worm;	
			//} 
			//else {
			//	// If not, then we'll say its a new worm and we will add it to the list of worms.
			//	all_worms.push_back(worm);
			//}

			//if (total_ergy[max_idx] > CtrlPanel.ergy_thres_mon) worm_to_modify.setCentroidGlobal(gbl_ctr_mass);
			//else worm_to_modify.setCentroidGlobal(Point(this_roi_x + this_roi_width / 2, this_roi_y + this_roi_height / 2)); // If the maximum energy of this Roi cannot reach a threshold, then append the center of this Roi

			////// In case of empty vector
			////if (max_idx != -1) vec_ergy.push_back(total_ergy[max_idx]);
			////else vec_ergy.push_back(0);

			//if (max_idx != -1) worm_to_modify.setEnergy(total_ergy[max_idx]);
			//else  worm_to_modify.setEnergy(0);
			
		}
		// TODO: Potentially implement a TRACK_ALL option

	}
	/*cout << "========= Paral end. Worm vec size:" << all_worms.size() << endl;
	for (Worm& w : all_worms) {
		cout << "Centroid:		" << w.getWormCentroid() << endl;
		cout << "Gbl Cent:		" << w.getWormCentroidGlobal() << endl;
		int small_img_x = w.getWormCentroidGlobal().x / 2.88;
		int small_img_y = w.getWormCentroidGlobal().y / 3.24;
		cout << "SmallImg:		" << Point(small_img_x, small_img_y) << endl << endl;
	}*/

	// cout << "Parallel Worm Finder time: " << timeFunc.getElapsedTime() << endl;
}

void WormFinder::updateWormPosition(Worm& worm, Point gbl_ctr_mass, double energy, int rad, bool is_lost) {
	// TODO : Check the glb center of mass against all the other worm locations in all_worms, making sure they're not within some small distance of eachother, to ensure that we're not doubling up on a worm.

	if (!is_lost) {
		worm.setGlbCentroidAndRoi(gbl_ctr_mass, rad, full_img_size);
		worm.setEnergy(energy);
		//worm.setRoiFromCentroidGlb(rad, full_img_size);
		//cout << "Worm tracked. Position has been updated to " << worm.getWormCentroidGlobal() << endl;
	}
	else {
		//cout << "No worm found. Worm has been lost" << endl;
	}
	worm.markLostStatus(is_lost);
}

void WormFinder::updateWormsAfterStageMove(double dx, double dy, double dydOx) {
	// Loop through all the worms and update their centroids based on the stage movement
	
	for (Worm& w : all_worms) {
		if (!w.is_visible) continue;
		Point curr_centroid = w.getWormCentroidGlobal();
		Point new_centroid = Point(curr_centroid.x - dx * dydOx, curr_centroid.y - dy * dydOx);
		w.setGlbCentroidAndRoi(new_centroid, ctrlPanel.rad_roi, full_img_size);

		// Check the new centroid to see if the stage moved and this worm is visible or not visible to the camera now.
		if (new_centroid.x < 0 || new_centroid.x > full_img_size.width || new_centroid.y < 0 || new_centroid.y > full_img_size.height) 
			w.is_visible = false;
		else 
			w.is_visible = true;
	}


}

void WormFinder::drawCirclesAtPoints(Mat& img_color, vector<Point> points, Scalar color) {
	if (img_color.type() != 16) {
		cvtColor(img_color, img_color, CV_GRAY2BGR);
	}

	for (Point p : points) {
		circle(img_color, p, 20, color, 2);
	}
}

// Member function that get the ROIs from vector of center pts and prevent out of bound
void WormFinder::getROIsfromPts(vector<Point> pts, Size full_sz, vector<Rect>& vec_Rois, int roi_radius) {

	for (int i = 0; i < pts.size(); i++) {

		// Set the ROI for each high energy centroid
		Rect ROI;
		ROI.x = pts[i].x - roi_radius;
		ROI.y = pts[i].y - roi_radius;
		ROI.width = 2 * roi_radius;
		ROI.height = 2 * roi_radius;

		// Prevent OOB
		ROI.x = max(ROI.x, 0);
		ROI.y = max(ROI.y, 0);
		ROI.width = min(ROI.width, full_sz.width - ROI.x);
		ROI.height = min(ROI.height, full_sz.height - ROI.y);

		vec_Rois.push_back(ROI);
	}
}

void WormFinder::prepControlPanel(ControlPanel& CP) {
	if (worm_finding_mode == FIND_ALL) {
		CP.beta = 1;
		CP.gamma = 1; 
		CP.delta = 0; 
		CP.miu_conf = 5.8;
		CP.sd_conf = 1.7;

		CP.ROIs.clear();
		
		// If we're in find all mode then check to see if we've already set up a specific ROI in which to
		// find worms. If not then we just use the entire image to find worms.
		if (CP.custom_finding_rois.size() != 0) {
			for (Rect roi : CP.custom_finding_rois) {
				CP.ROIs.push_back(roi);
			}
		}
		else {
			CP.ROIs.push_back(Rect(0, 0, full_img_size.width, full_img_size.height));
		}



	}
	else if (worm_finding_mode == TRACK_ONE) {
		CP.beta = 0; 
		CP.gamma = 1; 
		CP.delta = 0;
		CP.miu_conf = 0; 
		CP.sd_conf = 1;

		CP.ROIs.clear();
		CP.ROIs.push_back(tracked_worm->getWormRoi());
	}
	else if (worm_finding_mode == TRACK_ALL) {
		CP.beta = 0;
		CP.gamma = 1;
		CP.delta = 0;
		CP.miu_conf = 0;
		CP.sd_conf = 1;

		CP.ROIs.clear();
		for (Worm& w : all_worms) {
			if(w.is_visible)
				CP.ROIs.push_back(w.getWormRoi());
		}
	}

	CP.ergy_thres_mon = CP.gamma * (4 - CP.miu_conf) / CP.sd_conf;
}

vector<vector<Point>> WormFinder::getWormTrajectories(vector<Mat> trajectory_images) {
	worm_finding_mode = TRACK_ALL;

	// vector contining the worm positions at each frame
	vector<vector<Point>> worm_traj;

	for (int i = 0; i < trajectory_images.size(); i++) {

		prepControlPanel(ctrlPanel);
		ParalWormFinder(i, 0, trajectory_images, ctrlPanel);

		// Get the locations of all the worms in the current frame
		vector<Point> curr_worm_locs;
		for (Worm w : all_worms) {
			curr_worm_locs.push_back(w.getWormCentroidGlobal());
		}
		worm_traj.push_back(curr_worm_locs);

	}


	//if (!initial_pts.empty()) {
	//	worm_traj.push_back(initial_pts); // Append the initial positions

	//	// Iterate each frame to find the worm position under tracking mode
	//	ControlPanel CP1(ctrlPanel);

	//	for (int i = 0; i < vec_frms.size() - 1; i++) {

	//		// Debug show frame for debug

	//		//Mat debug_img;
	//		//vec_frms[i].copyTo(debug_img);
	//		//for (int j = 0; j < worm_traj[i].size(); j++) { drawCross(debug_img, worm_traj[i][j], colors::rr, 1, 8); }
	//		//namedWindow("Display frame 2", CV_WINDOW_AUTOSIZE);
	//		//imshow("Display frame 2", debug_img);
	//		//waitKey(1);

	//		setDmyTrajTracker(CP1, worm_traj[i]);
	//		worm_traj.push_back(DmyTrajTracker(CP1, vec_frms[i + 1]));

	//		// boost_sleep_ms(70);


	//	}
	//}

	return worm_traj;
}

void WormFinder::getPickableWormIdxs(vector<int>& pickable_worm_idxs) {
	for (int worm_idx = 0; worm_idx < all_worms.size(); worm_idx++) {
		Point worm_centroid = all_worms[worm_idx].getWormCentroidGlobal();
		double dist = ptDist(worm_centroid, pickable_region.pickable_center);
		if (dist > pickable_region.radius) {
			cout << "Worm outside of pickable region." << endl;
			continue;
		}
		else {
			pickable_worm_idxs.push_back(worm_idx);
			//worms.setTrackedWorm(&worms.all_worms[worm_idx]);
			//break;
		}
	}
}

void WormFinder::updatePickableRegion(Point3d current_pos, Point3d plate_pos, double dydOx) {
	pickable_region.pickable_center = pickable_region.original_center; // Reset the center as if there was no offset, so we can account for any new offset.

	// Found the offset of pickable region is off, therefore manually put a constant to the dx and dy
	double dx_pick_reg = 1.2 * (plate_pos.x - current_pos.x) * dydOx;
	double dy_pick_reg = 1.2 * (plate_pos.y - current_pos.y) * dydOx;
	pickable_region.pickable_center.x += dx_pick_reg;
	pickable_region.pickable_center.y += dy_pick_reg;

	cout << "$$$$$$$$$$$$$$$$$$$$$$$$$$$Pickable circle offset: " << dx_pick_reg << ", " << dy_pick_reg << endl;
}

// Takes a fluo image from the high mag camera and returns the number fluo worms it finds based on the number of bright (fluo) spots in the image.
int WormFinder::countHighMagFluoWorms(cv::Mat fluo_img, int thresh_val, int thresh_area, int fluo_spots_per_worm) {
	// Threshold fluo image
	//imshow("fluo_img", fluo_img);
	//waitKey(1);

	Mat norm, bw, bwfilt;

	//normalize(fluo_img, norm, 0, 255, NORM_MINMAX);
	//imshow("norm_img", fluo_img);

	threshold(fluo_img, bw, thresh_val, 255, CV_THRESH_BINARY);


	//imshow("Thresh_img", bw);
	//waitKey(1);

	// Remove small bright spots
	//erode(bw, bw, Mat::ones(Size(3, 3), CV_8U));

	// Filter objects and find number of objects
	int N_obj = filterObjects(bw, bwfilt, thresh_area, 9999);
	int num_fluo_worms = N_obj / fluo_spots_per_worm;

	cout << "Num worms: " << num_fluo_worms << ", num objs: " << N_obj << endl;
	//imshow("After filter", bwfilt);
	//waitKey(0);
	destroyWindow("fluo_img");
	destroyWindow("norm_img");
	destroyWindow("Thresh_img");
	destroyWindow("After filter");

	return num_fluo_worms;
}

int WormFinder::getclosestPoint(std::vector<cv::Point> pts_list, cv::Point ref_pt) {

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


bool WormFinder::findOnClick(int x, int y) {

	return true;

	//worm_finding_mode = FIND_ALL;

	//// Wait for the worm finder thread to finish finding the worms
	//// While we're waiting we can check to see if the pick is done heating
	//AnthonysTimer finding_timer;
	//while (worm_finding_mode != WAIT) {

	//	boost_sleep_ms(50);
	//	boost_interrupt_point();
	//}
	//cout << "Wormfinding took: " << finding_timer.getElapsedTime() << endl;

	//worm_finding_mode = TRACK_ALL;

	//return true;


	////////////// Worm Motel Masking test
	//// Testing segment worm method in Worm.cpp using the new Mask RCNN and client-server architecture
	//Worm test_worm;
	//String img_path = "C:\\Users\\Yuchi\\Desktop\\WM2_inverted.png";
	////String img_path = "C:\\Users\\lizih\\Dropbox\\WormPicker's shared workspace\\Mask_RCNN_dataset_shared\\Mask_RCNN_Data_Apr_2021\\03302021\\03302021_Adult_Herm\\herm_001\\high_mag_frames\\herm_001_21.png";
	//Mat worm_img = imread(img_path);
	//
	//int well_width = 132;
	//int well_height = 122;
	//int resize_multiplier = 4;

	//// Create the ROIs for each well. The position of each well is predictable inside the image, so the values are hard coded.
	//vector<Rect> wells;
	//vector<int> row_vals{ 26, 185, 344 };
	//vector<int> col_vals{ 24, 196, 374 };
	//for (int row_y : row_vals) {
	//	for (int col_x : col_vals) {
	//		wells.push_back(Rect(col_x, row_y, well_width, well_height));
	//	}
	//}

	//// Extract each ROI for each well, resize it so the worms are closer to the size of worms the network is used to seeing, and send it to the 

	//vector<Mat> masked_imgs;
	//for (Rect well : wells) {
	//	Mat well_img(worm_img(well));
	//	resize(well_img, well_img, Size(well_width * resize_multiplier, well_height * resize_multiplier));

	//	maskRCNN_results results = maskRcnnRemoteInference(well_img);
	//	results.worms_mask *= 255;
	//	masked_imgs.push_back(results.worms_mask);
	//}

	///*for (int i = 0; i < masked_imgs.size(); i++) {
	//	Rect bbox = boundingRect(masked_imgs[i]);
	//	rectangle(masked_imgs[i], bbox, colors::ww, 1);
	//	imshow("bbox", masked_imgs[i]);
	//	waitKey(0);
	//}*/

	//// Rebuild the whole image from each of the wells we extracted
	//Mat all_masks = Mat::zeros(worm_img.rows, worm_img.cols, CV_8UC1);

	//vector<Rect> bboxes;
	//for (int i = 0; i < masked_imgs.size(); i++) {
	//	resize(masked_imgs[i], masked_imgs[i], Size(masked_imgs[i].cols / resize_multiplier, masked_imgs[i].rows / resize_multiplier));
	//	bboxes.push_back(boundingRect(masked_imgs[i])); // Get the bounding box of the mask as well.
	//	masked_imgs[i].copyTo(all_masks(wells[i]));
	//}

	//// Mask each worm we identified with red
	//Mat all_masks_color;
	//cvtColor(all_masks, all_masks_color, CV_GRAY2RGB);
	//all_masks_color.setTo(Scalar(0, 0, 255), all_masks);

	//Mat worm_img_masked;
	//addWeighted(worm_img, 1.0, all_masks_color, 0.5, 0.0, worm_img_masked);

	//// Draw the bounding boxes
	//for (int i = 0; i < bboxes.size(); i++) {
	//	Rect corrected_bbox = Rect(bboxes[i].x + wells[i].x, bboxes[i].y + wells[i].y, bboxes[i].width, bboxes[i].height);
	//	rectangle(worm_img_masked, corrected_bbox, colors::rr, 1);
	//}


	//// Resize for convenience and display the results.
	//resize(worm_img, worm_img, Size(worm_img.cols * 1.75, worm_img.rows * 1.75));
	//imshow("orig", worm_img);
	//resize(worm_img_masked, worm_img_masked, Size(worm_img.cols, worm_img.rows));
	//imshow("masked", worm_img_masked);
	////imshow("all masks", all_masks_color);
	//waitKey(0);

	//return true;



	////////////
	////Mat mask_img = imread("C:\\Users\\pdb69\\Downloads\\PennFudanPed\\PennFudanPed\\PedMasks\\FudanPed00001_mask.png");
	////Mat mask_img = imread("C:\\Dropbox\\FramesData\\FramesForAnnotation\\train\\Renamed\\PedMasks\\027_contmask_final.png");
	//Mat mask_img = imread("C:\\Dropbox\\FramesData\\FramesForAnnotation\\train\\Renamed\\test\\027_mask.png");
	//Mat mask_out;

	//cout << "Img type: " << mask_img.type() << endl;
	////cout << mask_img << endl;
	//imshow("Mask", mask_img);
	//waitKey(0);

	//threshold(mask_img, mask_out, 4, 255, THRESH_BINARY);

	//imshow("Mask4", mask_out);
	//waitKey(0);

	//threshold(mask_img, mask_out, 3, 255, THRESH_BINARY);

	//imshow("Mask3", mask_out);
	//waitKey(0);

	//threshold(mask_img, mask_out, 2, 255, THRESH_BINARY);

	//imshow("Mask2", mask_out);
	//waitKey(0);

	//threshold(mask_img, mask_out, 1, 255, THRESH_BINARY);

	//imshow("Mask1", mask_out);
	//waitKey(0);

	//threshold(mask_img, mask_out, 0, 255, THRESH_BINARY);

	//imshow("Mask0", mask_out);
	//waitKey(0);


	


	//// PETES METHOD -------------------------
	//AnthonysTimer time_imgs;
	//Size low_s = img_low_mag_color.size();
	//Size high_s = img_high_mag_color.size();

	//Size total = Size(img_low_mag_color.size().width + img_high_mag_color.size().width, max(img_low_mag_color.size().height, img_high_mag_color.size().height));
	//
	////Mat both_imgs = Mat::ones(total, img_high_mag_color.type());
	//Mat both_imgs(total, img_high_mag_color.type(), Scalar(255, 255, 255));

	//int row_start = (total.height - low_s.height) / 2;
	///*int col_start = ((total.width - high_s.width) - low_s.width) / 2;

	//cout << "Row start: " << row_start << endl;
	//cout << "Col start: " << col_start << endl;

	//cout << "Row end: " << row_start + low_s.height << endl;
	//cout << "Col end: " << col_start + low_s.width << endl;*/
	//
	//img_low_mag_color.copyTo(both_imgs.rowRange(row_start, row_start + low_s.height).colRange(0, low_s.width));

	////cout << "Size: " << total.width << ", " << total.height << endl;

	//row_start = (total.height - high_s.height) / 2;
	////col_start = low_s.width + (total.width - high_s.width) / 2;

	////cout << "Row start: " << row_start << endl;
	////cout << "Col start: " << col_start << endl;

	////cout << "Row end: " << row_start + low_s.height << endl;
	////cout << "Col end: " << col_start + low_s.width << endl;

	//img_high_mag_color.copyTo(both_imgs.rowRange(row_start, row_start + high_s.height).colRange(low_s.width, total.width));

	//cout << "Pete's Method: " << time_imgs.getElapsedTime() << endl;

	//imshow("both imgs", both_imgs);
	//waitKey(0);

	//// ONLINE METHOD WITH CONCAT -----------------
	//time_imgs.setToZero();
	//Mat high_and_low;
	//Mat low_resize;
	////hconcat(img_high_mag_color, img_low_mag_color, high_and_low);
	//img_high_mag_color.copyTo(high_and_low);
	////high_and_low.push_back(img_low_mag_color);

	//resize(img_low_mag_color, low_resize, img_high_mag_color.size());

	//hconcat(high_and_low, low_resize, high_and_low);

	////high_and_low.push_back(low_resize);
	//cout << "Concat Method: " << time_imgs.getElapsedTime() << endl;
	//imshow("High Low", high_and_low);
	//waitKey(0);

	//imshow("low resize", low_resize);
	//waitKey(0);

	//cout << "In find on click method. Worm vector size: " << all_worms.size() << endl;
	//cout << x << "," << y << endl;
	////
	////ControlPanel control_panel(full_img_size, fonnx_worm); // Default
	//if (all_worms.size() == 0) {
	//	worm_finding_mode = FIND_ALL;

	//	while (worm_finding_mode != WAIT) {
	//		boost_sleep_ms(50);

	//	}

	//	cout << "In find on click method. Worm vector size: " << all_worms.size() << endl;
	//	worm_finding_mode = TRACK_ALL;
	//	cout << "Set to track all" << endl;
	//}
	//else {
	//	//432 x 342
	//	/*Worm x = Worm();
	//	x.setCentroidGlobal(Point(1244, 1108));
	//	x.setRoiFromCentroidGlb(80, full_img_size);
	//	all_worms.push_back(x);
	//	tracked_worm = &all_worms[all_worms.size()-1];*/
	//	tracked_worm = &all_worms[0];
	//	cout << "Attempting to track worm at location " << tracked_worm->getWormCentroidGlobal() << endl;

	////	/*Mat img_color;
	////	cvtColor(imggray, img_color, CV_GRAY2BGR);
	////	tracked_worm->drawWormPosition(img_color, colors::rr);

	////	showDebugImage(img_color, false, "Last location of worm to track", 0);*/

	//	worm_finding_mode = TRACK_ALL;

	//	//cout << "Attempting to draw circle on first worm" << endl;
	//	//tracked_worm->drawWormPosition(imggray);
	//	//drawAllWormPositions(imggray);
	//}

	///////// IMAGE SHIFT AND COMPARISON TESTS:
//	float data[50][50];
//	for (int i = 0; i < 50; i++) {
//		for (int j = 0; j < 50; j++) {
//
//			if (i == 49) {
//				data[i][j] = 255;
//			}
//			else {
//				data[i][j] = 0;
//			}
//
//		}
//	}
//
//
//	Mat test = Mat(50, 50, CV_32FC1, data);
//	Mat compare = Mat(50, 50, CV_32FC1, data);
//
//	Mat mat;
//	test.copyTo(mat);
//	
//
//	//Mat test;
//	//imggray.copyTo(test);
//	//Mat test2 = Mat(5, 50, CV_32FC1, Scalar(255));
//	int loops = 1;
//	////test.push_back(test2);
//	AnthonysTimer timeLoop;
//	//for (int i = 0; i < loops * test.rows; i++) {
//	//	Mat temp;
//	//	int k = (test.rows - 1);
//
//	//	Rect sub_img = Rect(0, 1, test.cols, k);
//
//	//	test.row(0).copyTo(temp);
//	//	test(sub_img).copyTo(test);
//	//	test.push_back(temp);
//
//	//	imshow("Test1", test);
//	//	waitKey(1);
//	//}
//
//	//cout << "My implementation: " << timeLoop.getElapsedTime() << endl;
//	
//
//	////float data2[1][test.cols];
//
//	//for (int i = 0; i < test.cols; i++) {
//	//	float temp = test.at<float>(49, i);
//	//	test.at<float>(49, i) = test.at<float>(0, i);
//	//	test.at<float>(0, i) = temp;
//	//}
//
//	//imshow("Test", test);
//	//waitKey(0);
//
////===========================
//
//	//shift the image to compare
//	for (int i = 0; i < 30; i++) {
//
//		Mat temp;
//		Mat m;
//		int k = (compare.rows - 1);
//		compare.row(k).copyTo(temp);
//
//		for (; k > 0; k--) {
//			m = compare.row(k);
//
//			/*imshow("m", m);
//			waitKey(0);*/
//
//			compare.row(k - 1).copyTo(m);
//
//			/*imshow("m2", m);
//			waitKey(0);*/
//		}
//		m = compare.row(0);
//		temp.copyTo(m);
//
//		imshow("compare post shift", compare);
//		waitKey(1);
//		boost_sleep_ms(100);
//	}
//
//	vector<Mat> results;
//	for (int i = 0; i < loops * mat.rows; i++) {
//
//		Mat result;
//		matchTemplate(mat, compare, result, TM_CCOEFF_NORMED);
//		//cout << "Corrcoef for shifting " << i << " pixel = " << result.at<float>(0, 0) << endl;
//		//cout << "Result size: " << result.size().height << ", " << result.size().width << endl;
//		results.push_back(result);
//
//
//		//
//		Mat temp;
//		Mat m;
//		int k = (mat.rows - 1);
//		mat.row(k).copyTo(temp);
//
//		for (; k > 0; k--) {
//			m = mat.row(k);
//
//			/*imshow("m", m);
//			waitKey(0);*/
//
//			mat.row(k - 1).copyTo(m);
//
//			/*imshow("m2", m);
//			waitKey(0);*/
//		}
//		m = mat.row(0);
//		temp.copyTo(m);
//		//
//		
//		imshow("mat post shift", mat);
//		waitKey(1);
//		boost_sleep_ms(100);
//	}
//	cout << "SO implementation: " << timeLoop.getElapsedTime() << endl;
//
//	int max_idx = -1;
//	float max_correlation = -999999;
//	for (int i = 0; i < results.size(); i++) {
//		cout << "pixel shift " << i << ": " << results[i].at<float>(0, 0) << endl;
//		if (results[i].at<float>(0, 0) > max_correlation) {
//			max_correlation = results[i].at<float>(0, 0);
//			max_idx = i;
//		}
//	}
//	cout << "Max correlation = " << max_correlation << endl;


	//while (worm_finding_mode != WAIT) {
	//	boost_sleep_ms(200);
	//}
	////ParalWormFinder(imggray, pre_frame_lo_mag, control_panel);
	////ParalWormFinder(imggray, imggray, CP0);

	//cout << "ParalWorm has run. Worm vector size: " << all_worms.size() << endl;
	// 
	//double min_dist = 999; // minimum distance from a contour to pickup location
	//int min_idx = 0;    //the index of the closest contour


	////find the closest worm to the click point
	//for (int i = 0; i < all_worms.size(); i++) {
	//	cout << "	" << x << "," << y << " ----- " << all_worms[i].getWormCentroid().x << "," << all_worms[i].getWormCentroid().y << endl;
	//	cout << "	" << x << "," << y << " ----- " << all_worms[i].getWormCentroidGlobal().x << "," << all_worms[i].getWormCentroidGlobal().y << endl;
	//	double this_dist = ptDist(Point(x, y), all_worms[i].getWormCentroid());

	//	if (this_dist < min_dist) {
	//		min_dist = this_dist;
	//		min_idx = i;
	//	}
	//}

	//if (all_worms.size() == 0) {
	//	return false;
	//}
	//tracked_worm = &(all_worms[min_idx]);

	//if (min_dist < track_radius) {
	//	//tracked_worm->reset(all_worms[min_idx].getContour());
	//	return true;
	//}
	//else {
	//	return false;
	//}
	//return false;
}



bool WormFinder::SetMoveOnClick(int x, int y) {

	loc_to_move_user_set = Point(x, y);
	cout << "The position to move is set to x = " << loc_to_move_user_set.x 
		<< ", y = " << loc_to_move_user_set.y << " by the user!"<< endl;
	return true;
}

void WormFinder::drawAllWormPositions(Mat& img_color, Scalar color, bool display_image_after) {
	//Mat img_color;
	if (img_color.type() != 16) {
		cvtColor(img_color, img_color, CV_GRAY2BGR);
	}

	//// Ensure that img is a CV_8UC3 (color 8 bit image), unless we're going to draw a new debug image
	//if (img_color.type() != CV_8UC3) {
	//	cout << "ERROR: drawSegmentation aborted because image was type " << img_color.type() << " instead of type 16  8UC3";
	//	return;
	//}
	for (Worm& w : all_worms) {
		w.drawWormPosition(img_color, full_img_size, color);
	}
	if (display_image_after) { showDebugImage(img_color, false, "Worm Positions", 0); }
	/*imshow("Worm Positions", img_color);
	waitKey(0);*/
}

// Measures the pixel difference in each bin
vector<int> WormFinder::measureBinPixDiff(std::vector<cv::Point>& coords, cv::Mat& Pixel_change, int bin_radius, bool get_bin_mot) {

	vector<int> pixel_diff_bins;

	//int bin_pixel_count = bin_radius * 4;
	

	string datetime_string = getCurrentTimeString(false, true);

	//motion_data << "Data collected at " << datetime_string << endl;

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

			/*if(sum > 0)
				motion_data << "Intensity Sum\t" << sum << "\tPixel Count\t" << sum / double(255) << endl;*/

			//// Check to see if this bin is mostly bright pixels (indicating an error in the difference image) and return if it does
			//if (sum / double(255) >= bin_pixel_count * double(0.75)) {
			//	pixel_diff_bins.push_back(-9999);
			//	return pixel_diff_bins;
			//}
		}
	}

	//motion_data << endl;

	return pixel_diff_bins;
}

std::vector<double> WormFinder::measureIntenBinClass(std::vector<cv::Point>& coords, cv::Mat& Roi_img, int bin_radius, int thresh, std::vector<cv::Mat>& bw, cv::Size blursz, cv::Size removesz, bool get_bin_class) {

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

void WormFinder::resetTrack() {
	tracked_worm->clear();
}

// look for a worm in the frame that matches the picked worm
// assume: the worm will move away from agar indentations
// thus, we will be able to find a contour of roughly the same area after a brief
// period of time

// For 10s maintain vector of worm shapes that are potential matches
// STEP 1: Constantly look for new worms, matching the centroid on each frame to a previous frame
// STEP 2: if it's the same worm, flow track it to see if it's moving
// STEP 3: start flow tracking on next frame

bool WormFinder::wasWormDropped() {
	AnthonysTimer timeout;
	int movementThreshold = 50; // TODO - this needs to be calibrated
	double areaErr = 0.1; //look for contours within +/-10% of picked worm 
	vector<Worm> potential_matches;

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
					Worm one for each worm
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


bool WormFinder::getWormNearestPickupLocCNN() {

	// Lock the object during this method to prevent manipulation of the current objects while attempting
	// to find the best object (causes crashes)
	StrictLock<WormFinder> guard(*this);


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
	while (tracked_worm->isEmpty_centroid() && Ta.getElapsedTime() < time_limit_s) {

		if (!tracked_worm->isEmpty_centroid()) {
			break;
		}
		boost::this_thread::sleep(boost::posix_time::milliseconds(10));

	}

	return !tracked_worm->isEmpty_centroid();
}


double WormFinder::getDistToPickup() const {
	return ptDist(tracked_worm->getWormCentroid(), getPickupLocation());
}

Point WormFinder::getPickupLocation() const {
	return pickup_location;
}

///********************************************************** Used in executeCenterFluoWorm in TestScript.cpp, which is also declared (but never defined) in ScriptActions. Will ExecuteCenterFluoWorm end up being a ScriptAction?
cv::Point WormFinder::getCtrOvlpFov() const {
	return Point(round(ovlpFov.x + 0.5*ovlpFov.width), round(ovlpFov.y + 0.5*ovlpFov.height));
}

void WormFinder::setPickupLocation(int x, int y) {
	pickup_location.x = x;
	pickup_location.y = y;
	return;
}

///******************************************************************* Only used in TestScripts.cpp
Worm* WormFinder::getTrackedWorm() const {
	return tracked_worm;
}

void WormFinder::setTrackedWorm(Worm* worm) {
	tracked_worm = worm;
}


///********************************************************** Used in executeCenterFluoWorm in TestScript.cpp, which is also declared (but never defined) in ScriptActions. Will ExecuteCenterFluoWorm end up being a ScriptAction?
Worm WormFinder::getHiMagWorm() const {
	return hiMag_worm;
}

Point WormFinder::getTrackedCentroid() const {
	if (tracked_worm == nullptr)
		return Point(-1, -1);
	return tracked_worm->getWormCentroidGlobal();
}

Rect WormFinder::getTrackedRoi() const {
	if (tracked_worm == nullptr)
		return Rect(-1, -1, -1, -1);
	return tracked_worm->getWormRoi();
}

vector<vector<Point>> WormFinder::getWormContours() const {
	vector<vector<Point>> result;
	for (int i = 0; i < all_worms.size(); i++) {
		result.push_back(all_worms[i].getContour());
	}
	return result;
}


///********************************************************** Only used in TestScript.cpp. Is commented out everywhere else.
std::vector<int> WormFinder::getPixelDiff() const {
	return pixel_change_rois;
}

///********************************************************** Used in unused quickpick methods in WormPicker.cpp. Also used in TestScripts.cpp
std::vector<double> WormFinder::getConfiSinglefrmCnn() const {
	return ConfiCentroid_singlefrmCnn;
}

int WormFinder::getNumBlobs() const {
	return (int)all_worms.size();
}

bool WormFinder::isWormNew(const Worm &worm, const vector<Worm> &worm_list, int
	_radius) {
	for (int i = 0; i < worm_list.size(); i++) {
		if (ptDist(worm.getWormCentroid(), worm_list[i].getWormCentroid()) < 20) {
			return false;
		}

	}
	return true;
}

bool WormFinder::isWormOnPickup() const {
	if (!tracked_worm) {
		return false;
	}

	if (getDistToPickup() <= max_dist_to_pickup) {
		return true;
	}
	else {
		return false;
	}
}

// Determine if a worm is centered to a taget location.
// If no target location in provided then the pickup location is used by default
// If no worm location is provided then the tracked worm's location is used by default.
bool WormFinder::isWormCentered(Point target_loc, Point worm_loc, double max_dist) const {

	if (all_worms.size() == 0 || (worm_loc == Point(-1, -1) && tracked_worm->isEmpty_centroid())) {
		return false;
	}

	if (target_loc == Point(-1, -1)) { target_loc = getPickupLocation(); }
	if (worm_loc == Point(-1, -1)) { worm_loc = tracked_worm->getWormCentroidGlobal(); }
	if (max_dist == -1) { max_dist = max_dist_to_pickup; }

	double dist_to_target = ptDist(worm_loc, target_loc);
	//cout << "dist to target = " << dist_to_target << endl;

	if (dist_to_target <= max_dist) {
		return true;
	}
	else {
		return false;
	}
}


bool WormFinder::isWormTracked() const {
	return tracked_worm != nullptr;
}


void WormFinder::UpdateMonitorRois(std::vector<cv::Rect> vec_rois) {
	Monitored_ROIs.clear();
	for (int i = 0; i < vec_rois.size(); i++) {
		Monitored_ROIs.push_back(vec_rois[i]);
	}
}

/*
	Verifies if a worm has been picked or dropped successfully.
	Will return the number of worms it finds near the pick after the picking or dropping action.
	Note that the number of expected worms at the destination plate should only ever be 1. If it is more than 1 then we should be
	picking to an intermediate plate first to isolate the worm we want.
*/
int WormFinder::VerifyWormPickOrDrop(int num_expected_worms, bool verify_drop_over_whole_plate) {
	verifying_pick_action = true;
	// Wait a bit for the agar to settle, record images while we wait so we can track the worm back to right after it was dropped
	AnthonysTimer agarTimer;
	double time_to_settle = 2;
	double time_to_high_mag_grab = 1.4;
	bool high_mag_grabbed = false;
	vector<Mat> pre_recorded_images;
	while (agarTimer.getElapsedTime() < time_to_settle) {
		if (!high_mag_grabbed && agarTimer.getElapsedTime() > time_to_high_mag_grab) {
			// Hopefully the agar has settled grab a high mag image to try to use for drop verification on Mask RCNN network.
			img_high_mag_color.copyTo(worm_drop_verification);
			high_mag_grabbed = true;
		}
		pre_recorded_images.push_back(imggray.clone());
		boost_sleep_ms(200);
	}

	// NOTE: commented out the below code in favor of simply searching the entire image for worms, and then backtracking each of those worms
	// to its origin point at the time of the drop, and then checking those origins vs their distance to the drop off location to see if they're terminate enough.
	// This will be slightly slower, but will eliminate the problem of the worm crawling quickly out of the search ROI. If this turns out to
	// be too slow then perhaps we can revert back to the reduced ROI method later.

	// we are only interested in finding worms around the high mag FOV, 
	// so create a custom ROI for worm finding centered around the high mag FOV
	/*int max_dist_to_worm = 60;
	int length = max_dist_to_worm * 2;*/
	
	//verify_roi = Rect(high_mag_fov_center_loc.x - max_dist_to_worm, high_mag_fov_center_loc.y - max_dist_to_worm, length, length);
	//ctrlPanel.custom_finding_rois.push_back(verify_roi); 
														   

	image_timer.setToZero(); // Now that the agar is settled, start recording images for find all mode.
	worm_finding_mode = FIND_ALL;

	while (worm_finding_mode != WAIT) {
		boost_sleep_ms(50);
	}

	cout << "Drop verification find all mode found " << all_worms.size() << " worms." << endl;

	int curr_img_in_queue = most_recent_img_idx;
	vector<Mat> recorded_images = image_queue;
	
	vector<Mat> historical_images;
	// Place the images into the vector so that the first image is the most recent and the last image is the oldest. (go back in time)
	for (int i = 0; i < recorded_images.size(); i++) {
		historical_images.push_back(recorded_images[curr_img_in_queue]);
		curr_img_in_queue = curr_img_in_queue == 0 ? recorded_images.size() - 1 : curr_img_in_queue - 1;	
	}

	for (int i = pre_recorded_images.size() - 1; i >= 0; i--) {
		historical_images.push_back(pre_recorded_images[i]);
	}

	// First try to just look at the image from the high mag camera and try to find a worm in there with the python network
	// Note that we don't do this right off the bat because if it fails we need to try the trajectory method, and finding the worms
	// and gathering the images to be used in the trajectory method is time sensitive. So we gather first, then try the python network,
	// then try the trajectory method if the python network doesnt find anything.

	cout << "Trying python network for drop verification." << endl;
	maskRCNN_results results = maskRcnnRemoteInference(worm_drop_verification);
	double minVal;
	double maxVal;
	Point minLoc;
	Point maxLoc;
	minMaxLoc(results.worms_mask, &minVal, &maxVal, &minLoc, &maxLoc);

	Mat worm_m(results.worms_mask);
	worm_m *= 50;

	string datetime_string = getCurrentTimeString(false, true);
	string verif_img_fpath = "C:\\WormPicker\\recordings\\verification_images\\drop_verification\\" + datetime_string;
	/*imshow("Drop verif image", worm_drop_verification);
	imshow("Mask RCNN result", worm_m);
	waitKey(0);*/

	if (maxVal >= num_expected_worms) {
		// TODO: The network is sometimes finding worms from artifacts in the image that are not worms.
		// We can try to add a minimum contour size using "contour = isolateLargestContour(mask, min_contour_size)" as similar to when we segment a worm
		// But this could go both ways... if the worm only has a portion of its tail showing because it crawled a bit, then the Mask RCNN might find it and
		// get rejected due to contour size.

		// if the network didn't find any worms then it will send back a completely black mask (all pixel values = 0)
		cout << "Worm drop verified with python network!" << endl;
		//verify_roi = Rect(-1, -1, 1, 1);
		recent_verification_technique = "Mask RCNN";
		string verif_mask = verif_img_fpath + "_success_mask.png";
		string verif_img = verif_img_fpath + "_success_img.png";

		vector<int> params{ CV_IMWRITE_PNG_COMPRESSION, 0 };
		imwrite(verif_mask, worm_m, params);
		imwrite(verif_img, worm_drop_verification, params);

		verifying_pick_action = false;
		return (int)maxVal;
	}

	string verif_mask = verif_img_fpath + "_fail_mask.png";
	string verif_img = verif_img_fpath + "_fail_img.png";

	vector<int> params{ CV_IMWRITE_PNG_COMPRESSION, 0 };
	imwrite(verif_mask, worm_m, params);
	imwrite(verif_img, worm_drop_verification, params);

	// If we failed to find the worm in the high mag camera then it could have crawled away too fast, so we can use the trajectory method.
	cout << "Trying trajectory method for drop verification." << endl;

	// Now we want to track the worms we found back in time using TRACK_ALL mode.
	// We don't want to modify this current wormfinder's worm list with these worm positions (because they are not the ACTUAL CURRENT positions of the worms)
	// so we create a new wormfinder and use that to call parallelwormfinder directly on the historical images.
	WormFinder trajectoryFinder(full_img_size, watching_for_picked, fonnx_worm, imggray, img_high_mag, img_high_mag_color, img_low_mag_color);
	trajectoryFinder.all_worms = all_worms;


	// Trajectory analysis: back-tracing the traveling path of this worm in the frame history
	vector<vector<Point>> worm_positions_backwards = trajectoryFinder.getWormTrajectories(historical_images); 

	// Switch the axis of vectors
	vector<vector<Point>> worm_trajectories;
	SwapAxis(worm_positions_backwards, worm_trajectories);

	// To check whether these is a worm crawl out from unloading site.
	bool wrm_crwl_out = false;
	int backtracked_worms = 0;

	if (!worm_trajectories.empty()) {

		origin_traj.clear();
		for (int i = 0; i < worm_trajectories.size(); i++) {

			origin_traj.push_back(worm_trajectories[i].back());
			double DisToPk = ptDist(origin_traj[i], pickup_location);
			cout << "Worm " << i << "crawling from point x = " << origin_traj[i].x << ", y = " << origin_traj[i].y;
			cout << "Dist. to unloading pt = " << DisToPk << endl;

			int verify_radius = (verify_drop_over_whole_plate) ?  verify_radius_over_whole_plate : verify_radius_near_pick;

			if (DisToPk <= verify_radius) {
				//wrm_crwl_out = true;
				//recent_verification_technique = "Backtracked Trajectories";
				backtracked_worms++;
				cout << "Worm drop verified with trajectory method!" << endl;
			}
		}
	}
	else {
		cout << "No worms found during verification step!" << endl;
		wrm_crwl_out = false;
	}

	//verify_roi = Rect(-1, -1, 1, 1);
	int worms_found = 0;
	if (backtracked_worms >= maxVal) {
		worms_found = backtracked_worms;
		recent_verification_technique = "Backtracked Trajectories";
	}
	else {
		worms_found = maxVal;
		recent_verification_technique = "Mask RCNN";
	}

	verifying_pick_action = false;
	return worms_found;
}
