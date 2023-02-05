#pragma once
/*
	Worm.h

	Adapted from WormShape files originally implemented by Alexander Kassouni
	The Worm.cpp/.h files are meant to replace the old WormShape.cpp/.h files, which were overrun with deprecated and unused code.
	If a different file is looking in this file for a method that is not here (perhaps an old commented out or unused method needs to be in use again) then
	that method is most likely going to be in the WormShape files and can be copied from there into here to remedy the problem.
	
	Peter Bowlin
	Fang-Yen Lab
	Janurary 2021


	Worm one for each worm
	increase a time specifying how long it has been tracked for. Timer in constructor
		-->If can't update, error is thrown, option to do something like delete it or do nothing.

*/

#ifndef Worm_H
#define Worm_H

#include <vector>
#include "opencv2\opencv.hpp"
#include "AnthonysTimer.h"
#include "AnthonysCalculations.h"
#include "boost/optional.hpp"
#include "WormExceptions.h"



// TODO: Erase old duplicates of these in Worms.h

//class WormLostException : public std::exception {
//	virtual const char* what() const throw() {
//		return "Worm was not found";
//	}
//};
//
//class EmptyWormException : public std::exception {
//	virtual const char* what() const throw() {
//		return "Worm is empty, expected otherwise";
//	}
//};

struct maskRCNN_gender_info {
	cv::Point head_centroid;
	cv::Point tail_centroid;
	int gender;
	float head_score;
	float tail_score;

	maskRCNN_gender_info() {
		head_centroid = cv::Point(-1, -1);
		tail_centroid = cv::Point(-1, -1);
		gender = 0;
		head_score = 0.0;
		tail_score = 0.0;
	}
};

struct maskRCNN_results {
	cv::Mat worms_mask;
	int num_worms;
	std::vector<maskRCNN_gender_info> gender_info;
};


// Structure for phenotype parameters
enum sex_types { UNKNOWN_SEX, HERM, MALE, DUMMY_COUNT_SEX_TYPE};
static const char* sexTypeStrings[] = { "UNKNOWN_SEX", "HERM", "MALE", "DUMMY_COUNT_SEX_TYPE" };
enum GFP_types { UNKNOWN_GFP, GFP, NON_GFP, DUMMY_COUNT_GFP_TYPE };
static const char* gfpTypeStrings[] = { "UNKNOWN_GFP", "GFP", "NON_GFP", "DUMMY_COUNT_GFP_TYPE" };
enum RFP_types { UNKNOWN_RFP, RFP, NON_RFP, DUMMY_COUNT_RFP_TYPE};
static const char* rfpTypeStrings[] = { "UNKNOWN_RFP", "RFP", "NON_RFP", "DUMMY_COUNT_RFP_TYPE" };
enum morph_types { UNKNOWN_MORPH, NORMAL, DUMPY, DUMMY_COUNT_MORPH_TYPE};
static const char* morphTypeStrings[] = { "UNKNOWN_MORPH", "NORMAL", "DUMPY", "DUMMY_COUNT_MORPH_TYPE" };
enum stage_types { UNKNOWN_STAGE, ADULT, L4_LARVA, L3_LARVA, L2_LARVA, L1_LARVA, DUMMY_COUNT_STAGE_TYPE};
static const char* stageTypeStrings[] = { "UNKNOWN_STAGE", "ADULT", "L4_LARVA", "L3_LARVA", "L2_LARVA", "L1_LARVA", "DUMMY_COUNT_STAGE_TYPE" };

class Phenotype{

public:
	sex_types SEX_type;
	GFP_types GFP_type;
	RFP_types RFP_type;
	morph_types MORPH_type;
	stage_types STAGE_type;
	double max_GFP_inten = -1; // Maximum GFP intensity of the worm. Equals to -1 if not assigned the value
	double max_RFP_inten = -1; // Maximum RFP intensity of the worm. Equals to -1 if not assigned the value

	Phenotype();
	Phenotype(sex_types sex_type, GFP_types gfp_type, RFP_types rfp_type, morph_types morph_type, stage_types stage_type);
	bool is_empty () const;
	bool WriteToFile(std::ofstream& f, std::string note = "") const;

	bool operator==(const Phenotype& other) const {
		return SEX_type == other.SEX_type && GFP_type == other.GFP_type && RFP_type == other.RFP_type
			&& MORPH_type == other.MORPH_type && STAGE_type == other.STAGE_type;
	}
	bool operator!=(const Phenotype& other) const {
		return !(other == *this);
	}
	bool operator< (const Phenotype& other) const {
		return (SEX_type < other.SEX_type) ||
		(SEX_type == other.SEX_type && GFP_type < other.GFP_type) ||
		(SEX_type == other.SEX_type && GFP_type == other.GFP_type && RFP_type < other.RFP_type) ||
		(SEX_type == other.SEX_type && GFP_type == other.GFP_type && RFP_type == other.RFP_type && MORPH_type < other.MORPH_type) ||
		(SEX_type == other.SEX_type && GFP_type == other.GFP_type && RFP_type == other.RFP_type && MORPH_type == other.MORPH_type && STAGE_type < other.STAGE_type);
	}
	bool operator> (const Phenotype& other) const { return other < *this; }
	bool operator<= (const Phenotype& other) const { return !(*this > other); }
	bool operator>= (const Phenotype& other) const { return !(*this < other); }
};

class Worm {
public:
	// construct an empty Worm
	Worm();

	//construct a Worm from the given contour
	Worm(const std::vector<cv::Point> &_contour);

	~Worm();

	// Load the worm intensity template if it hasn't already been loaded by a previous Worm
	void loadIntensityTemplate();

	/*
			Worm segmentation and geometry
	*/

	/* Main function for segmenting a worm in an image. Supply an
	   image (or ROI of an image) containing exactly one worm.
	   Example: Full frame from the fluo camera, or partial frame
	   from main camera.

	   Input image is modified by findWormsInImage and mus tbe a copy.
	 */
	bool segmentWorm(cv::Mat img, int bin_num = 30, double width = 0.45, bool show_debug = false);
	bool PhenotypeWormFromMaskRCNNResults(cv::Mat& img, maskRCNN_results& results, 
		int worm_num = 0, int bin_num = 30, double width = 0.45, bool show_debug = false, cv::Mat* ptr_gfp_frm = nullptr, cv::Mat* ptr_rfp_frm = nullptr, bool single_worm_in_img = false);



	/*
		Helper functions for worm segmentation
	*/

	/// measure intensity profile on a fluorescence image (for example),  using the already
	/// segmented geometry for this worm (must be done right after segmentWorm)
	void fluoProfile(const cv::Mat& fluo_img);

	/// Segment fluorescence objects in the fluo_image without any concern for the worm's BF segmentation if any
	//bool segmentFluo(cv::Mat fluo_img, int thresh_val = 30, int thresh_area = 10);

	/// Geometry helpers
	std::vector<double> getAngles(const std::vector<cv::Point>& wormBry); // Get the angles of contour
	void getCandidateHeadTail(const std::vector<cv::Point>& wormBdry, int& loc1, int& loc2, double mini_dist = 0.3);
	void flipWormGeometry(); /// Flip head and tail and all member data

	///Get drosal/ventral boundary - NOT ventral/dorsal disambiguated.
	void Worm::splitContour(const std::vector<cv::Point>& wholeBry, int& loc1, int& loc2, std::vector<cv::Point>& DrlBdry, std::vector<cv::Point>& VtrlBdry);
	void getCoordMask(std::vector<cv::Mat>& Masks, cv::Size imgsz, const std::vector<cv::Point>& Bdry1, const std::vector<cv::Point>& Bdry2, double width = 1);
	void getBinMeanInten(std::vector<double>& outvec, const cv::Mat& inputary, const std::vector<cv::Mat>& Masks);
	void getCenterline(std::vector<cv::Point>& ctrline, const std::vector<cv::Point>& Bdry1, const std::vector<cv::Point>& Bdry2);
	
	/// Disambiguate head/tail either using intensity template or gender/head/tail CNN
	/// Select which method to use by passing in the argument types
	bool checkHeadTail(const std::vector<double>& templates, const std::vector<double>& body_inten);
	bool checkHeadTail(const cv::Mat& img, cv::dnn::Net net);

	void classifyDumpy(cv::Size img_size);
	void classifyStageByArea(const cv::Mat& worm_mask);
	void classifyStageByLeng(int WormLeng, cv::Size& img_size);
	bool PhenotypeFluo(cv::Mat* fluo_img, cv::Mat& mask, int thresh_val, int thresh_area, int iter, bool show_debug = false, bool single_worm_in_img = false, double* max_fluo_inten = nullptr);


	std::vector<double> bf_profile, fluo_profile;
	std::deque<std::vector<double>> bf_profile_history;


	/// Draw the current Worm's segmentation on an image. 
	/// Set debug to true to display the image right away
	void drawSegmentation(cv::Mat& img_color, bool debug = false);
	void drawWormPosition(cv::Mat& img_color, cv::Size full_img_size, cv::Scalar color);

	// update the Worm geometry from the given contour
	void Worm::updateContour(const std::vector<cv::Point> &_contour);
	void Worm::updateContourV2(const std::vector<cv::Point> &_contour); // Test version 

	// reset the Worm geometry
	// initial area and time tracked are reset
	void Worm::reset(const std::vector<cv::Point> &_contour);

	//Reset the centroid
	void Worm::setCentroid(cv::Point& centroid_assign);
	void Worm::setGlbCentroidAndRoi(cv::Point& centroid_assign, int roi_radius, cv::Size img_size);

	void Worm::setEnergy(double energy);

	// Set the ROI
	void Worm::setRoi(cv::Rect& roi_assign);

	// Calculate the worm's ROI based on its current global coordinates
	//void Worm::setRoiFromCentroidGlb(int roi_radius, cv::Size img_size);

	void Worm::markLostStatus(bool lost);

	// reset initial area and time tracked but keep the contour
	void Worm::reset();

	// set the Worm to empty
	void Worm::clear();

	// update the sex of the worm
	void Worm::updateSex(int step);

	bool checkEdgeProximity(cv::Size img_size, int edge_proximity_limit);
	// returns if the Worm is empty
	// bool isEmpty() const;
	// This is not safe because tracked worm is a pointer to vector that might have no elements
	// Instead, use tracked_worm == true or false to determine whether pointer is null
	// We might need to write future functions like "isContourEmpty, but that's a different purpose
	// Than whether any worm is tracked at all.

	// returns if the centroid of Worm is empty
	bool isEmpty_centroid() const;

	// returns if the roi of Worm is empty
	bool Worm::isEmpty_roi() const;

	//returns the distance travelled by the worm
	float getDistanceTraveled() const;

	//returns the worms centroid
	cv::Point getWormCentroid() const;
	cv::Point getWormCentroidGlobal() const;

	double getWormEnergy();

	//returns the roi of tracked worm
	cv::Rect getWormRoi() const;

	// returns the worms contour
	std::vector<cv::Point> getContour() const;

	// returns the initial area of the worm from the first contour
	float Worm::getInitialArea() const;

	// returns the current area of the worm
	float Worm::getArea();
	float Worm::getAreaV2(); // Test function

	cv::Rect Worm::boundingBox() const;

	float Worm::getDisplacement() const;
	bool is_lost = false; // Tracks whether or not the CNN failed to find this worm at its last known location.
	bool is_visible = true; // Tracks whether or not the worm is visible in the camera based on the current stage position
	double worm_length;

	Phenotype current_phenotype; // Phenotype of the current worm

	double dumpy_ratio_thresh = 0.142; /// The ratio of Area/Length^2 above which we think the worm is a dumpy. This threshold was determined experimentally.
	double dumpy_ratio = 0; /// This worm's ratio of Area/Length^2, to determine if it is dumpy

protected:
	// Worm geometry (High res camera relative coordinates)

	/// Contours and centerline
	std::vector<cv::Point> contour;
	std::vector<cv::Point> dorsal_contour;
	std::vector<cv::Point> ventral_contour;
	std::vector<cv::Point> center_line;
	std::vector<cv::Point> cnn_contour; /// Contour of the worm-activated CNN area.

	/// Intensity as a function of body coordinate
	std::vector<cv::Mat> regional_masks;

	/// Key points along the worm centerline
	cv::Point pt_mid;
	cv::Point pt_head;
	cv::Point pt_tail;
	int idx_head, idx_tail; /// Index along contour for head and tail
	std::deque<cv::Point> head_history; /// History of last 20 head positions
	cv::Point avg_head_pos; /// Moving average head position
	cv::Rect rect_head;		///rectangle for head, same size as used for classification
	cv::Rect rect_tail;		/// rectangle for tail	

	/// Parameters and measurements about the worms
	int N_ctr_line_bins;
	double mini_dist;
	double width;
	double kLhead, kLtail; /// Normalized curvature of cline at the head and tail
	bool is_male;
	int min_contour_size; /// How long the contour needs to be to proceed with segmentation
	int contour_jump; /// How much to jump (or smooth) as getting curvature of the contour
	
	

	/// Worm intensity template for head-tail identification
	static std::vector<double> intensity_template;

	/// Worm CNN for high magnification (DEPRECATED - REPLACED WITH MASK-RCNN)
	static cv::dnn::Net picker_net_hi_mag;

	/// Head/Tail/Gender CNN
	static cv::dnn::Net picker_net_gender;

	// Worm speed and location (from main camera)
	cv::Point centroid;		/// Location in the current field of view
	cv::Point centroid_global; /// Location relative to whole-plate coordinate

	double worm_energy;



	float distanceTraveled;
	AnthonysTimer trackTimer;

	cv::Rect roi; // ROI of tracking region after a worm has been targeted
	boost::optional<float> area;
	float initial_area;
	float initial_sex;
	float sex;
	cv::Point initial_position;
};

#endif
