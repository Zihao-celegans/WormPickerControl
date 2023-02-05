#include "Worm.h"
#include "ImageFuncs.h"
#include "AnthonysCalculations.h"
#include "showDebugImage.h"
#include "DnnHelper.h"
#include "mask_rcnn.h"


using namespace std;
using namespace cv;
using json = nlohmann::json;

Phenotype::Phenotype() {
	SEX_type = UNKNOWN_SEX;
	GFP_type = UNKNOWN_GFP;
	RFP_type = UNKNOWN_RFP;
	MORPH_type = UNKNOWN_MORPH;
	STAGE_type = UNKNOWN_STAGE;
}


Phenotype::Phenotype(
	sex_types sex_type, 
	GFP_types gfp_type, 
	RFP_types rfp_type, 
	morph_types morph_type, 
	stage_types stage_type):

	SEX_type(sex_type),
	GFP_type(gfp_type),
	RFP_type(rfp_type),
	MORPH_type(morph_type),
	STAGE_type(stage_type)
{}

bool Phenotype::is_empty() const {

	return(
		SEX_type == UNKNOWN_SEX && 
		GFP_type == UNKNOWN_GFP &&
		RFP_type == UNKNOWN_RFP &&
		MORPH_type == UNKNOWN_MORPH &&
		STAGE_type == UNKNOWN_STAGE
		);
}


bool Phenotype::WriteToFile(ofstream& f, string note) const {
	if (!f.is_open()) { return false; cout << "Cannot write phenotype to a closed file!" << endl; }
	f << "SEX_type" << "\t" << (sex_types)SEX_type << "\t"
		<< "GFP_type" << "\t" << (GFP_types)GFP_type << "\t"
		<< "max_GFP_inten" << "\t" << (double)max_GFP_inten << "\t"
		<< "RFP_type" << "\t" << (RFP_types)RFP_type << "\t"
		<< "max_RFP_inten" << "\t" << (double)max_RFP_inten << "\t"
		<< "MORPH_type" << "\t" << (morph_types)MORPH_type << "\t"
		<< "STAGE_type" << "\t" << (stage_types)STAGE_type << "\t";

	// Allow to put additional note to the file
	if (note.size() > 0) { f << "Note" << "\t" << note << "\t"; }
	
	f << endl;

	return true;
}


// Static variable definitions
std::vector<double>  Worm::intensity_template = {};
dnn::Net Worm::picker_net_hi_mag = dnn::readNetFromONNX(filenames::cnn_picker_hi_mag);
//dnn::Net Worm::picker_net_gender = dnn::readNetFromONNX(filenames::cnn_picker_gender);
dnn::Net Worm::picker_net_gender = dnn::readNetFromONNX(
	filenames::cnn_picker_hi_mag); // TODO: WARNING: WRONG NETWORK, ENTERED JUST TO COMPILE, WILL CRASH IF TRY TO RUN

// Constructors
//*********************************************** Not used
Worm::Worm() {
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
	
	// Load the intensity template (first method of discriminating h/t)
	loadIntensityTemplate();

	// Load the gender/head/tail CNN


	assert(contour_jump <= min_contour_size, "contour_jump must be <= to min_contour_size");
}

Worm::Worm(const std::vector<cv::Point> &_contour) {
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

Worm::~Worm() {
	return;
}

// Load the worm intensity template if it hasn't already been loaded by a previous Worm
void Worm::loadIntensityTemplate() {

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

std::vector<double> Worm::getAngles(const std::vector<cv::Point>& wormBry) {
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
void Worm::getCandidateHeadTail(const std::vector<cv::Point>& wormBdry, int& loc1, int& loc2, double min_rel_dist) {

	// min_rel_dist: the minimum distance (fraction of the vector length) between two sharpest angles
	// Therefore, cannot be larger than 0.5
	// Default value is 0.3

	vector<double> contourAngles = getAngles(wormBdry);
	/*cout << "worm angles" << endl;
	for (int i = 0; i < contourAngles.size(); i++) {
		cout << i << ": " << contourAngles[i] << endl;
	}*/

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

void Worm::flipWormGeometry() {

	// If current orientation is not correct, flip the entire anatomy
	int temp = idx_tail;
	idx_tail = idx_head;
	idx_head = temp;

	pt_head = contour[idx_head];
	pt_tail = contour[idx_tail];

	reverse(dorsal_contour.begin(), dorsal_contour.end());
	reverse(ventral_contour.begin(), ventral_contour.end());
	reverse(bf_profile.begin(), bf_profile.end());
	reverse(center_line.begin(), center_line.end());

}


bool Worm::segmentWorm(cv::Mat img, int bin_num, double width, bool show_debug) {

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

	// Mask RCNN Segmentation method
	maskRCNN_results results = maskRcnnRemoteInference(img);
	Mat mask = results.worms_mask;
	
	/////////////////// TODO: Return false if multiple worms found, but not if the "multiple" are just overlaps
	mask = mask == 1; // Select the first worm found, should be label 1
	//showDebugImage(mask, false);

	if (show_debug) {
		cv::imshow("DEBUG", mask);
		waitKey(0);
	}

	contour = isolateLargestContour(mask, min_contour_size);

	// Return false if no large contour found
	if (contour.size() < min_contour_size)
		return false;

	///* STEP 2: refine boundaries of worm using flowPass */
	//// contour = flowPass(img, contour);  Doesn't make much better than just thresholding


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

	// get the centerline skeleton
	getCenterline(center_line, dorsal_contour, ventral_contour);


	// Double check with assesshead and tail!!!!
	// Get the mask of each bin along the body
	getCoordMask(regional_masks, Size(img.cols, img.rows), dorsal_contour, ventral_contour, width);

	// Get the intensity plot along the body coordinate
	getBinMeanInten(bf_profile, img, regional_masks);

	// Ensure that the intensity template is resampled to the same length as the intensity curves
	intensity_template = vectorDownsample(intensity_template, bin_num);


	// Disambiguate head and tail by comparing the intensity profile to a template
	//bool correct_orientation = checkHeadTail(intensity_template, bf_profile);
	//if (!correct_orientation) // Copy the anatomy to output values
	//	flipWormGeometry();
	// pt_head = contour[idx_head];
	// pt_tail = contour[idx_tail];

	// Disambiguate the head and tail by CNN. Also finds the gender.
	//bool correct_orientation = checkHeadTail(img, picker_net_gender);
	//if (!correct_orientation){ flipWormGeometry(); }

	// Get the head and tail from the mask rcnn results
	// Since we take only the first worm returned then we only grab the first gender results from the worm info vector. (this info corresponds to the first worm)
	maskRCNN_gender_info worm_gender_info = results.gender_info[0];
	current_phenotype.SEX_type = (sex_types)worm_gender_info.gender;
	pt_head = worm_gender_info.head_centroid;
	pt_tail = worm_gender_info.tail_centroid;
	if (pt_head.x != -1 && pt_head.y != -1) {
		int r = 25;
		rect_head = Rect(pt_head.x - r, pt_head.y - r, 2 * r, 2 * r);
	}
	else {
		rect_head = Rect();
	}

	if (pt_tail.x != -1 && pt_tail.y != -1) {
		int r = 25;
		rect_tail = Rect(pt_tail.x - r, pt_tail.y - r, 2 * r, 2 * r);
	}
	else {
		rect_tail = Rect();
	}


	

	/// Store location of head and tail



	//// track the last 20 identified head locations, and swap head-tail if head not closest
	///// (Note the last-20 is purely as identified above, not including this continuity check
	//head_history.push_back(pt_head);
	//if (head_history.size() > 15) { head_history.pop_front(); }

	///// Compare historical position to current and flip if needed
	//vector<Point> head_history_vect = { head_history.begin(),head_history.end() };
	//avg_head_pos = contourMedian(head_history_vect);
	/////if (ptDist(avg_head_pos, pt_head) > ptDist(avg_head_pos, pt_tail)) 
	/////	flipWormGeometry();




	// Calculate the length of the worm based on its centerline
	worm_length = arcLength(center_line, false);

	return true;

}

// Worm num is 0 indexed, so if you want the first worm (corresponding to pixel values 1 in the mask) then worm num is 0.
bool Worm::PhenotypeWormFromMaskRCNNResults(cv::Mat& img, maskRCNN_results& results, int worm_num, int bin_num, double width, bool show_debug, cv::Mat* ptr_gfp_frm, cv::Mat* ptr_rfp_frm, bool single_worm_in_img) {

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

	Mat mask;
	results.worms_mask.copyTo(mask);

	/////////////////// TODO: Return false if multiple worms found, but not if the "multiple" are just overlaps
	mask = mask == worm_num + 1; // if there are multiple worms in the mask we isolate the worm we want
	//showDebugImage(mask, false);

	if (show_debug) {
		Mat debug_img(mask);
		debug_img *= 50;
		double min_val;
		double max_val;
		Point min_loc;
		Point max_loc;
		cv::minMaxLoc(debug_img, &min_val, &max_val, &min_loc, &max_loc);
		cout << "min: " << min_val << ", max: " << max_val << endl;
		cv::imshow("DEBUG", debug_img);
		waitKey(0);
	}

	contour = isolateLargestContour(mask, min_contour_size);

	// Return false if no large contour found
	if (contour.size() < min_contour_size) {
		cout << "No large contour found while segmenting the worm" << endl;
		return false;
	}
		

	///* STEP 2: refine boundaries of worm using flowPass */
	//// contour = flowPass(img, contour);  Doesn't make much better than just thresholding

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

	// get the centerline skeleton
	getCenterline(center_line, dorsal_contour, ventral_contour);


	// Double check with assesshead and tail!!!!
	// Get the mask of each bin along the body
	getCoordMask(regional_masks, Size(img.cols, img.rows), dorsal_contour, ventral_contour, width);

	// Get the intensity plot along the body coordinate
	getBinMeanInten(bf_profile, img, regional_masks);

	// Ensure that the intensity template is resampled to the same length as the intensity curves
	intensity_template = vectorDownsample(intensity_template, bin_num);


	// Disambiguate head and tail by comparing the intensity profile to a template
	//bool correct_orientation = checkHeadTail(intensity_template, bf_profile);
	//if (!correct_orientation) // Copy the anatomy to output values
	//	flipWormGeometry();
	// pt_head = contour[idx_head];
	// pt_tail = contour[idx_tail];

	// Disambiguate the head and tail by CNN. Also finds the gender.
	//bool correct_orientation = checkHeadTail(img, picker_net_gender);
	//if (!correct_orientation){ flipWormGeometry(); }

	// Get the head and tail from the mask rcnn results
	current_phenotype.SEX_type = UNKNOWN_SEX;
	pt_head = Point(-1, -1);
	pt_tail = Point(-1, -1);
	if (results.gender_info.size() != 0) {
		maskRCNN_gender_info& worm_gender_info = results.gender_info[worm_num];
		current_phenotype.SEX_type = (sex_types)worm_gender_info.gender;
		pt_head = worm_gender_info.head_centroid;
		pt_tail = worm_gender_info.tail_centroid;

		int angle_threshold = 110;
		//Check to see if the head or tail is missing from the mask rcnn results
		if (pt_head.x == -1 && pt_tail.x != -1) {
			cout << "%%%%%%%%%%%%%%% Missing Head! Using geometry instead." << endl;
			// No head was found, so we'll assign it to the location along the worm curve where the angle is sharpest (that is not near the tail)
			vector<double> contourAngles = getAngles(contour);
			Point loc1 = contour[idx_head];
			Point loc2 = contour[idx_tail];
			double dist_loc1 = abs(ptDist(pt_tail, loc1));
			double dist_loc2 = abs(ptDist(pt_tail, loc2));

			Point head_loc = (dist_loc1 > dist_loc2 ? loc1 : loc2);
			double head_loc_angle = (dist_loc1 > dist_loc2 ? contourAngles[idx_head] : contourAngles[idx_tail]);

			if (head_loc_angle < angle_threshold) {
				cout << "Viable head found using geometry." << endl;
				cout << "Geo head loc: " << head_loc << endl;
				pt_head = head_loc;
				worm_gender_info.head_centroid = head_loc;
				worm_gender_info.head_score = -1; // This score will indicate that the head was determined using geometry and not the mask rcnn network.
				cout << "new worm gender head loc: " << worm_gender_info.head_centroid << endl;
			}
			else {
				cout << "Could not find the missing head using geometry" << endl;
			}

		}
		else if (pt_tail.x == -1 && pt_head.x != -1) {
			cout << "%%%%%%%%%%%%%%% Missing Tail! Using geometry instead." << endl;
			// No tail was found, so we'll assign it to the location along the worm curve where the angle is sharpest (that is not near the head)
			vector<double> contourAngles = getAngles(contour);
			Point loc1 = contour[idx_head];
			Point loc2 = contour[idx_tail];
			double dist_loc1 = abs(ptDist(pt_head, loc1));
			double dist_loc2 = abs(ptDist(pt_head, loc2));

			Point tail_loc = (dist_loc1 > dist_loc2 ? loc1 : loc2);
			double tail_loc_angle = (dist_loc1 > dist_loc2 ? contourAngles[idx_tail] : contourAngles[idx_head]);

			if (tail_loc_angle < angle_threshold) {
				cout << "Viable tail found using geometry." << endl;

				pt_tail = tail_loc;
				worm_gender_info.tail_centroid = tail_loc;
				worm_gender_info.tail_score = -1; // This score will indicate that the tail was determined using geometry and not the mask rcnn network.

				//NOTE THAT THIS DOES NOT ACTUALLY DETERMINE THE GENDER OF THE WORM. WE SHOULD ATTEMPT TO DISAMBIGUATE THE GENDER AT SOME POINT.
			}
			else {
				cout << "Could not find the missing tail using geometry" << endl;
			}
		}
	}

	if (pt_head.x != -1 && pt_head.y != -1) {
		int r = 25;
		rect_head = Rect(pt_head.x - r, pt_head.y - r, 2 * r, 2 * r);
	}
	else {
		rect_head = Rect();
	}

	if (pt_tail.x != -1 && pt_tail.y != -1) {
		int r = 25;
		rect_tail = Rect(pt_tail.x - r, pt_tail.y - r, 2 * r, 2 * r);
	}
	else {
		rect_tail = Rect();
	}


	/// Store location of head and tail

	//// track the last 20 identified head locations, and swap head-tail if head not closest
	///// (Note the last-20 is purely as identified above, not including this continuity check
	//head_history.push_back(pt_head);
	//if (head_history.size() > 15) { head_history.pop_front(); }

	///// Compare historical position to current and flip if needed
	//vector<Point> head_history_vect = { head_history.begin(),head_history.end() };
	//avg_head_pos = contourMedian(head_history_vect);
	/////if (ptDist(avg_head_pos, pt_head) > ptDist(avg_head_pos, pt_tail)) 
	/////	flipWormGeometry();


	// Calculate the length of the worm based on its centerline
	worm_length = arcLength(center_line, false);

	// Determine if the worm is dumpy
	classifyDumpy(mask.size());

	// Determine stage of worm
	//classifyStageByArea(mask); // By size
	classifyStageByLeng(worm_length, mask.size()); // By length

	// GFP phenotyping
	if (ptr_gfp_frm != nullptr) {
		// Load parameters for intensity and area threshold for phenotyping
		json GFP_phenotype_config = Config::getInstance().getConfigTree("phenotyping")["GFP"];
		int intensity_threshold = GFP_phenotype_config["intensity"].get<int>();
		int area_threshold = GFP_phenotype_config["area"].get<int>();
		int iter = GFP_phenotype_config["iter"].get<int>();

		// Update worm GFP property
		// If stage is unknown, means the worm is too close to edge of FOV
		// Then set the fluorescence type to unknown if no bright spot detected (might be the fluorescence cutoff by the image boundary)
		// But set to GFP if bright spot detected

		double max_GFP_inten = -1;
		bool have_GFP_bright_spot = PhenotypeFluo(ptr_gfp_frm, mask, intensity_threshold, area_threshold, iter, false, single_worm_in_img, &max_GFP_inten);
		current_phenotype.max_GFP_inten = max_GFP_inten;

		if (have_GFP_bright_spot) {
			current_phenotype.GFP_type = GFP;
		}
		else if (current_phenotype.STAGE_type == UNKNOWN_STAGE || current_phenotype.MORPH_type == UNKNOWN_MORPH) {
			current_phenotype.GFP_type = UNKNOWN_GFP;
		}
		else {
			current_phenotype.GFP_type = NON_GFP;
		}

	}

	// RFP phenotyping
	if (ptr_rfp_frm != nullptr) {
		// Load parameters for intensity and area threshold for phenotyping
		json RFP_phenotype_config = Config::getInstance().getConfigTree("phenotyping")["RFP"];
		int intensity_threshold = RFP_phenotype_config["intensity"].get<int>();
		int area_threshold = RFP_phenotype_config["area"].get<int>();
		int iter = RFP_phenotype_config["iter"].get<int>();

		// Update worm RFP property
		// If stage is unknown, means the worm is too close to edge of FOV
		// Then set the fluorescence type to unknown if no bright spot detected (might be the fluorescence cutoff by the image boundary)
		// But set to RFP if bright spot detected

		double max_RFP_inten = -1;
		bool have_RFP_bright_spot = PhenotypeFluo(ptr_rfp_frm, mask, intensity_threshold, area_threshold, iter, false, single_worm_in_img, &max_RFP_inten);
		current_phenotype.max_RFP_inten = max_RFP_inten;

		if (have_RFP_bright_spot) {
			current_phenotype.RFP_type = RFP;
		}
		else if (current_phenotype.STAGE_type == UNKNOWN_STAGE || current_phenotype.MORPH_type == UNKNOWN_MORPH) {
			current_phenotype.RFP_type = UNKNOWN_RFP;
		}
		else {
			current_phenotype.RFP_type = NON_RFP;
		}
	}

	cout << "Gender: " << current_phenotype.SEX_type << endl;
	cout << "GFP: " << current_phenotype.GFP_type << endl;
	cout << "max_GFP_inten:	" << current_phenotype.max_GFP_inten << endl;
	cout << "RFP: " << current_phenotype.RFP_type << endl;
	cout << "max_RFP_inten:	" << current_phenotype.max_RFP_inten << endl;
	cout << "Morphology: " << current_phenotype.MORPH_type << endl;
	cout << "Stage: " << current_phenotype.STAGE_type << endl;

	/*imshow("high mag", img);
	imshow("mask", mask);
	waitKey(0);
	destroyWindow("high mag");
	destroyWindow("mask");*/

	return true;

}

// Check to see if the worm mask is too terminate to the edge of the mask (meaning the worm may have been cut off in the image)
bool Worm::checkEdgeProximity(Size img_size, int edge_proximity_limit) {
	
	Rect bbox = boundingBox();
	if (bbox.x < edge_proximity_limit || bbox.x + bbox.width > img_size.width - edge_proximity_limit
		|| bbox.y < edge_proximity_limit || bbox.y + bbox.height > img_size.height - edge_proximity_limit) {
		cout << "Worm is too close to the edge of the image!" << endl;
		return true;
	}
	return false;
}

void Worm::classifyDumpy(Size img_size) {
	if (checkEdgeProximity(img_size, 20)) {
		current_phenotype.MORPH_type = UNKNOWN_MORPH;
		return;
	}

	// Get the area of the worm
	float area = getArea();
	// Calculate the ratio
	dumpy_ratio = area / (worm_length * worm_length);

	cout << "Worm A/L^2 ratio: " << dumpy_ratio << endl;
	// Check if the worm is dumpy
	current_phenotype.MORPH_type = (dumpy_ratio > dumpy_ratio_thresh) ? DUMPY : NORMAL;
}

void Worm::classifyStageByArea(const Mat& worm_mask) {
	int num_worm_pixels = countNonZero(worm_mask);
	cout << "Number pixels for stage: " << num_worm_pixels << endl;

	// Pixel value threshold for each stage size were determined from the staging length data we gathered.
	// These results are dependent on the worm pixel confidence threshold we use to determine if a pixel
	// belongs in a mask. The worm pixel confidence was 0.5 when we recorded the staging data, so if 
	// we change it to something else then the staging determination will need to be adjusted accordingly.
	// The worm pixel confidence the mask rcnn network uses can be found in the RCNN_segment_image.py file in ActivityAnalyzer.

	if (num_worm_pixels > 10485) {
		// Adult Herm
		current_phenotype.STAGE_type = ADULT;
		if (current_phenotype.SEX_type != HERM){
			current_phenotype.SEX_type = HERM;
			cout << "Worm gender has been overidden to Herm based on mask size." << endl;
		}
		return;
	}

	if (current_phenotype.SEX_type == MALE) {
		current_phenotype.STAGE_type = ADULT; // Technically we could have L4_LARVA males, but stage is not really important when dealing with Males so we'll just say its an adult.
		return;
	}

	// Check to see if the worm mask is too terminate to the edge of the mask (meaning the worm may have been cut off in the image)
	if (checkEdgeProximity(worm_mask.size(), 20)) {
		// Bounding box is too terminate to the edge to determing the stage
		current_phenotype.STAGE_type = UNKNOWN_STAGE;
		return;
	}
	
	if (num_worm_pixels > 4030) {
		current_phenotype.STAGE_type = L4_LARVA; // Technically in this size range, if we don't know if its male or herm because we failed to identify the gender with the network... 
					// we could have L4_LARVA herm, adult male, or L4_LARVA male here. But again, because stage doesnt really matter for Males we can safely label it L4_LARVA, rather than unknown stage.
	}
	else if (num_worm_pixels > 1975) {
		current_phenotype.STAGE_type = L3_LARVA;
	}
	else if (num_worm_pixels > 1090) {
		current_phenotype.STAGE_type = L2_LARVA;
	}
	else {
		current_phenotype.STAGE_type = L1_LARVA;
	}

}


void Worm::classifyStageByLeng(int WormLeng, cv::Size& img_size) {

	cout << "Worm Length: " << WormLeng << endl;

	if (WormLeng > 300 && WormLeng < 489) { // if the length is too long then something must be wrong
		// Adult
		current_phenotype.STAGE_type = ADULT;
		return;
	}

	if (current_phenotype.SEX_type == MALE) {
		current_phenotype.STAGE_type = ADULT; // Technically we could have L4_LARVA males, but stage is not really important when dealing with Males so we'll just say its an adult.
		return;
	}

	// Check to see if the worm mask is too terminate to the edge of the mask (meaning the worm may have been cut off in the image)
	if (checkEdgeProximity(img_size, 20)) {
		// Bounding box is too terminate to the edge to determing the stage
		current_phenotype.STAGE_type = UNKNOWN_STAGE;
		return;
	}

	if (WormLeng > 207 && WormLeng <= 300) {
		current_phenotype.STAGE_type = L4_LARVA; // Technically in this size range, if we don't know if its male or herm because we failed to identify the gender with the network... 
					// we could have L4_LARVA herm, adult male, or L4_LARVA male here. But again, because stage doesnt really matter for Males we can safely label it L4_LARVA, rather than unknown stage.
	}

	else if (WormLeng > 163 && WormLeng <= 207) {
		current_phenotype.STAGE_type = L3_LARVA;
	}

	else if (WormLeng > 121 && WormLeng <= 163) {
		current_phenotype.STAGE_type = L2_LARVA;
	}

	else if (WormLeng > 0 && WormLeng <= 121){
		current_phenotype.STAGE_type = L1_LARVA;
	}

	else {
		current_phenotype.STAGE_type = UNKNOWN_STAGE;
	}

	return;
}

bool Worm::PhenotypeFluo(cv::Mat* fluo_img, cv::Mat& mask, int thresh_val, int thresh_area, int iter, bool show_debug, bool single_worm_in_img, double* max_fluo_inten) {
	// Threshold fluo image
	//imshow("fluo_img", fluo_img);
	//waitKey(1);

	Mat norm, bw, bwfilt;

	//normalize(fluo_img, norm, 0, 255, NORM_MINMAX);
	//waitKey(2000);

	threshold(*fluo_img, bw, thresh_val, 255, CV_THRESH_BINARY);


	//imshow("Thresh_img", bw);
	//waitKey(1);

	// Remove small bright spots
	//erode(bw, bw, Mat::ones(Size(3, 3), CV_8U));

	// Filter objects and find number of objects
	filterObjects(bw, bwfilt, thresh_area, 9999);

	// Dialate the mask contour a bit for compensating worm moving
	Mat dialted_mask;
	if (!single_worm_in_img) 
	{ dilate(mask, dialted_mask, getStructuringElement(MORPH_CROSS, Size(3, 3)), Point(-1, -1), iter); }
	else 
	{
		dialted_mask = 255 * Mat::ones(mask.size(), CV_8UC1);
	}

	// Get the maximum of pixel value of the fluorescence frame of the worm
	if (max_fluo_inten != nullptr) {
		Mat overlap_fluo_frm;
		bitwise_and(*fluo_img, dialted_mask, overlap_fluo_frm);

		double minVal;
		double maxVal;
		Point minLoc;
		Point maxLoc;

		minMaxLoc(overlap_fluo_frm, &minVal, &maxVal, &minLoc, &maxLoc);

		*max_fluo_inten = maxVal;
	}


	// Find any bright spots falling inside the worm contour
	Mat overlap_bright_spot;
	bitwise_and(bwfilt, dialted_mask, overlap_bright_spot);
	int num_bright_in_contour = countNonZero(overlap_bright_spot);


	if (show_debug) {
		// Show debug images
		imshow("Thresh_img", bw);
		waitKey(2000);
		imshow("After filter", bwfilt);
		waitKey(2000);
		imshow("mask", mask);
		waitKey(2000);
		imshow("dialted_mask", dialted_mask);
		waitKey(2000);
		imshow("overlap", overlap_bright_spot);
		waitKey(2000);

		//cout << "Num worms: " << num_fluo_worms << ", num objs: " << N_obj << endl;
		//imshow("After filter", bwfilt);
		//waitKey(0);
		destroyWindow("Thresh_img");
		destroyWindow("After filter");
		destroyWindow("mask");
		destroyWindow("dialted_mask");
		destroyWindow("overlap");
	}

	// If any bright pixel within contour then we think it is fluorescent worm
	return (num_bright_in_contour > 0);
}


//// Deprecated in favor of having this method in WormFinder as countHighMagFluoWorms
//bool Worm::segmentFluo(Mat fluo_img, int thresh_val, int thresh_area) {
//
//	// Threshold fluo image
//	imshow("fluo_img", fluo_img);
//	waitKey(1);
//
//
//	Mat bw, bwfilt;
//	threshold(fluo_img, bw, thresh_val, 255, CV_8U);
//
//
//	imshow("Thresh_img", bw);
//	waitKey(1);
//
//	// Remove small bright spots
//	//erode(bw, bw, Mat::ones(Size(3, 3), CV_8U));
//
//	// Filter objects and find number of objects
//	int N_obj = filterObjects(bw, bwfilt, thresh_area, 9999);
//
//	// Return if not one fluo object is found
//	if (N_obj == 0) {
//		cout << "No fluo objects found" << endl;
//		return false;
//	}
//	else if (N_obj > 1) {
//		cout << "Found too many fluo objects (" << N_obj << ")" << endl;
//		return true;
//	}
//	else {
//		cout << "Fouce ONE fluo object!" << endl;
//		return true;
//	}
//}

void Worm::fluoProfile(const Mat& fluo_img) {

	// Get the intensity plot along the body coordinate
	getBinMeanInten(fluo_profile, fluo_img, regional_masks);

}


// This function is NOT ventral/dorsal disambiguated. TO DO: disambiguate dorsal and ventral for other applications
void Worm::splitContour(const std::vector<cv::Point>& wholeBry, int& loc1, int& loc2, std::vector<cv::Point>& DrlBdry, std::vector<cv::Point>& VtrlBdry) {

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


void Worm::getCoordMask(std::vector<cv::Mat>& Masks, const cv::Size imgsz, const std::vector<cv::Point>& Bdry1, const std::vector<cv::Point>& Bdry2, double width) {


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
	Masks.push_back(Masks[Masks.size() - 1]);

}


void Worm::getBinMeanInten(std::vector<double>& outvec, const cv::Mat& inputary, const std::vector<cv::Mat>& Masks) {

	//cout << "Mean Intensity in each bin:" << endl;

	for (int i = 0; i < Masks.size(); i++) {
		cv::Scalar scalar = cv::mean(inputary, Masks[i]);
		//cout << scalar[0] << endl;
		outvec.push_back(scalar[0]);
	}
}

// Get the center line of worm skeleton
void Worm::getCenterline(std::vector<cv::Point>& ctrline, const std::vector<cv::Point>& Bdry1, const std::vector<cv::Point>& Bdry2) {
	for (int i = 0; i < Bdry1.size(); i++) {
		ctrline.push_back(Point((Bdry1[i].x + Bdry2[i].x)*0.5, (Bdry1[i].y + Bdry2[i].y)*0.5));
	}
}

// Improved version of head and tail disambiguation - intensity method
// Based on an empirical rule: image intensity shows different patterns along the body
// Use scattering pattern matching (correlation coefficient) to measure whether current orientation is correct.
// Details see weekly updates on 2020 Oct 5
bool Worm::checkHeadTail(const std::vector<double>& templates, const std::vector<double>& body_inten) {

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

// Head-tail disambiguation AND gender determination using CNN

bool Worm::checkHeadTail(const cv::Mat& img, cv::dnn::Net net) {

	// Select points near the putative head and tail, but walked back
	// slightly so that more of the worm body gets into the crop_image
	int offset = 1;
	Point p0 = center_line[offset];
	Point p1 = center_line[center_line.size() - offset - 1];


	// Extract 70x70 images around the putative head and tail
	int r = 25;
	vector<Rect> rects = { Rect(p0.x - r,p0.y - r,2 * r,2 * r), Rect(p1.x - r, p1.y - r, 2 * r, 2 * r) };
	vector<int> classes = { -1,-1 };
	vector<vector<double>> probs = { vector<double>{} , vector<double>{} };
	vector<Point> pts = { p0, p1 };
	cv::Mat blob;

	for (int i = 0; i < rects.size(); i++) {

		// Enforce rects in bounds by moving the anchorpoint only (not height/width)
		Rect R = rects[i];
		R.x = max(R.x, 0);
		R.y = max(R.y, 0);

		if (R.x + R.width > img.cols - 1)
			R.x -= R.x + R.width - img.cols;

		if (R.y + R.height > img.rows - 1)
			R.y -= R.y + R.height - img.rows;


		// Extract image, then Resize back to 70x70 if had to adjust bounds
		Mat crop_img;
		img(R).copyTo(crop_img);

		if (crop_img.rows != 2 * r || crop_img.cols != 2 * r) {
			resize(crop_img, crop_img, Size(70, 70));
		}

		// Convert crop image to 3 channel if not already
		if (crop_img.type() != CV_8UC3 && crop_img.type() != CV_32FC3) {
			cvtColor(crop_img, crop_img, COLOR_GRAY2BGR);
		}

		// Convert crop image to 32F and normalize from 0 to 1
		crop_img.convertTo(crop_img, CV_32F,1/255.0);
		normalize(crop_img, crop_img, 0, 1, NORM_MINMAX);

		// Feed normalized crop image to clasifier
		//vector<Mat> img_v = { crop_img };
		//CnnOutput out = classifyImages(img_v, net);
		classes[i] = classifyImage(crop_img, net, blob, probs[i]);


		// Assign head/tail and gender info if successfully recognized
		//showDebugImage(img, false);
		//showDebugImage(crop_img, false);
		// If either image is ruled "background", segmentation is failure.
		 //Or if the two images get the same class
		 //Or if the two images are both tails 2/3
		bool fail_conditions =	(classes[i] == 0) ||
								(i>0 & classes[i] == classes[0]) ||
								(i>0 & classes[i] >= 2 && classes[0] >=2);
		//showDebugImage(crop_img);
		if (false) { //fail_conditions
			rect_head = Rect();
			rect_tail = Rect();
			current_phenotype.SEX_type = UNKNOWN_SEX;
			return false;
		}

		/*/// Class 3: Head (0 for ht only net)
		else if (classes[i] == 3) {
			rect_head = R;
			pt_head = pts[i];
		}

		/// Class 1-2 - herm or male tail
		else if (classes[i] == 2 || classes[i] == 1) {
			rect_tail = R;
			pt_tail = pts[i];
			gender = HERM;
		}

		/// Class 2 = male tail
		if (classes[i] == 2) {
			rect_tail = R;
			pt_tail = pts[i];
			gender = MALE;
		}*/
	}

	/* 
		Alternative H/T/G assignment - H-T orientation to the highest total confidence
		Valid combinations:
		Head (3),		Male Tail (2)
		Head (3),		Herm Tail (1)
		Male Tail (2),	Head (3)
		Herm Tail (1),	Head (3)

	*/

	double max_combo_score = -5;
	int max_combo_idx = -1;
	vector<vector<int>> combos = { {3,2},{3,1},{2,3},{1,3} }; // Side 0, Side 1 classes in each sub-bracket. 

	for (int i = 0; i < combos.size(); i++) {

		// Get class number from the list of valid combinations
		int class0 = combos[i][0];
		int class1 = combos[i][1];

		// Compute the score total for this combination
		double this_prob = probs[0][class0] + probs[1][class1];

		// Is it the maximum? Remember that
		if (this_prob > max_combo_score) {
			max_combo_score = this_prob;
			max_combo_idx = i;
		}
	}

	// Assign head, tail, gender according to highest combo
	vector<int> top_combo = combos[max_combo_idx];
	int head_side = vectorFind(top_combo, 3);
	int tail_side = !head_side; // Works for idx 0 vs 1 only in top_combo size = 2

	/// Head
	rect_head = rects[head_side];
	pt_head = pts[head_side];

	/// Tail
	rect_tail = rects[tail_side];
	pt_tail = pts[tail_side];

	/// gender
	if (top_combo[tail_side] == 2)
		current_phenotype.SEX_type = MALE;

	else if (top_combo[tail_side] == 1)
		current_phenotype.SEX_type = HERM;



	return true;

}



/// Draw the current Worm's segmentation on an image. Adapted from the former Worms -> PhenotypeFluo
// ************************************************************************ Only used in TestScripts.... but this feels useful and like something we should keep?
void Worm::drawSegmentation(cv::Mat& img_color, bool debug) {

	// Ensure that img is a CV_8UC3 (color 8 bit image), unless we're going to draw a new debug image
	if (img_color.type() != CV_8UC3) {
		cout << "ERROR: drawSegmentation aborted because image was type " << img_color.type() << " instead of type 16  8UC3";
		return;
	}

	// Draw tail
	if (current_phenotype.SEX_type == MALE)
		putText(img_color, "Tail (Male)", Point(rect_tail.x + rect_tail.width+3, rect_tail.y), 
			FONT_HERSHEY_SIMPLEX, 0.75, colors::rr);

	else if (current_phenotype.SEX_type == HERM)
		putText(img_color, "Tail [Herm]", Point(rect_tail.x + rect_tail.width + 3, rect_tail.y), 
			FONT_HERSHEY_SIMPLEX, 0.75, colors::rr);
	
	///circle(img_color, pt_tail, 10, colors::rr, 2);
	rectangle(img_color, rect_tail, colors::rr, 1);

	// Draw head
	putText(img_color, "Head", Point(rect_head.x + rect_head.width + 3, rect_head.y), 
		FONT_HERSHEY_SIMPLEX, 0.75, colors::bb);
	///drawCross(img_color, pt_head, colors::gg, 2, 15);
	rectangle(img_color, rect_head, colors::bb, 1);
	///circle(img_color, avg_head_pos, 10, colors::gg, -1);


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

void Worm::drawWormPosition(Mat& img_color, Size full_img_size, Scalar color) {
	/*Mat img_color;
	cvtColor(img, img_color, CV_GRAY2BGR);*/

	//// Ensure that img is a CV_8UC3 (color 8 bit image), unless we're going to draw a new debug image
	//if (img_color.type() != CV_8UC3) {
	//	cout << "ERROR: drawSegmentation aborted because image was type " << img_color.type() << " instead of type 16  8UC3";
	//	return;
	//}
	Size draw_img_size = img_color.size();

	//Translate the global coordinates onto the draw image in case the draw image is a different size than the full image
	double h_multiplier = double(draw_img_size.height) / double(full_img_size.height);
	double w_multiplier = double(draw_img_size.width) / double(full_img_size.width);

	Point draw_loc = Point2d(centroid_global.x * w_multiplier, centroid_global.y * h_multiplier);

	circle(img_color, draw_loc, 10, color, 1);

	/*showDebugImage(img_color, false, "DEBUG", 2);*/
	
}

//Setter Functions

void Worm::updateContour(const vector<Point> &_contour) {
	contour = _contour;
	area = boost::none;
	distanceTraveled += ptDist(centroid, getCentroid(contour));
	centroid = getCentroid(contour);
	return;
}

void Worm::updateContourV2(const std::vector<cv::Point> &_contour) {
	contour = _contour;
	return;
}

//*********************************************************************** Not used
void Worm::updateSex(int step) {
	//sex =  calculateSexLikelihood(step);
}

void Worm::reset(const vector<Point> &_contour) {
	updateContour(_contour);
	initial_position = getWormCentroid();
	initial_area = getArea();
	distanceTraveled = 0;
	trackTimer.setToZero();
}

void Worm::setCentroid(cv::Point& centroid_assign) {
	centroid = centroid_assign;
}

void Worm::setGlbCentroidAndRoi(cv::Point& centroid_assign, int roi_radius, Size img_size) {
	centroid_global = centroid_assign;

	// Set the ROI
	roi.x = centroid_global.x - roi_radius;
	roi.y = centroid_global.y - roi_radius;
	roi.width = 2 * roi_radius;
	roi.height = 2 * roi_radius;

	// Prevent OOB
	roi.x = max(roi.x, 0);
	roi.y = max(roi.y, 0);
	roi.width = min(roi.width, img_size.width - roi.x);
	roi.height = min(roi.height, img_size.height - roi.y);
}

void Worm::setEnergy(double energy) {
	worm_energy = energy;
}

void Worm::setRoi(cv::Rect& roi_assign) {
	roi = roi_assign;
}

//void Worm::setRoiFromCentroidGlb(int roi_radius, cv::Size img_size) {
//	roi.x = centroid_global.x - roi_radius;
//	roi.y = centroid_global.y - roi_radius;
//	roi.width = 2 * roi_radius;
//	roi.height = 2 * roi_radius;
//
//	// Prevent OOB
//	roi.x = max(roi.x, 0);
//	roi.y = max(roi.y, 0);
//	roi.width = min(roi.width, img_size.width - roi.x);
//	roi.height = min(roi.height, img_size.height - roi.y);
//}

void Worm::markLostStatus(bool lost) {
	is_lost = lost;
}

void Worm::reset() {
	reset(contour);
}

void Worm::clear() {
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

bool Worm::isEmpty_centroid() const {
	return (centroid == Point(-1, -1));
}

bool Worm::isEmpty_roi() const {
	return (roi.x == -1 && roi.y == -1);
}

float Worm::getDistanceTraveled() const {
	return distanceTraveled;
}

cv::Point Worm::getWormCentroid() const {
	return centroid;
}

cv::Point Worm::getWormCentroidGlobal() const {
	return centroid_global;
}

double Worm::getWormEnergy() {
	return worm_energy;
}

cv::Rect Worm::getWormRoi() const {
	return roi;
}

std::vector<cv::Point> Worm::getContour() const {
	return contour;
}

float Worm::getArea() {

	if (area) {
		return area.get();
	}
	else {
		if (contour.empty()) {
			area = 0;
			return 0;
		}
		area = contourArea(contour);
		return area.get();
	}

}


//********************************************************** This is only used in TestScript
float Worm::getAreaV2() {

	if (!contour.empty()) {
		area = contourArea(contour);
		return area.get();
	}
	else {
		area = 0;
		return 0;
	}
}

Rect Worm::boundingBox() const {
	return boundingRect(contour);
}

float Worm::getInitialArea() const {
	return initial_area;
}

float Worm::getDisplacement() const {
	return ptDist(initial_position, getWormCentroid());
}



