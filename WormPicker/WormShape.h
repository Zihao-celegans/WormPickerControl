/*
	WormShape.h
	Alexander Kassouni
	Fang-Yen Lab
	June 2019

				
	WormShape one for each worm
	increase a time specifying how long it has been tracked for. Timer in constructor
		-->If can't update, error is thrown, option to do something like delete it or do nothing.
				
*/
#pragma once
#ifndef WORMSHAPE_H
#define WORMSHAPE_H

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




class WormShape {
public:
	// construct an empty WormShape
	WormShape();
	
	//construct a WormShape from the given contour
	WormShape(const std::vector<cv::Point> &_contour);

	~WormShape();

	// Load the worm intensity template if it hasn't already been loaded by a previous WormShape
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
	bool segmentWorm(cv::Mat img, int bin_num = 30, double width = 0.45);
	
	

	/*
		Helper functions for worm segmentation
	*/

	/// measure intensity profile on a fluorescence image (for example),  using the already
	/// segmented geometry for this worm (must be done right after segmentWorm)
	void fluoProfile(const cv::Mat& fluo_img);

	/// Geometry helpers
	std::vector<double> getAngles(const std::vector<cv::Point>& wormBry); // Get the angles of contour
	void getCandidateHeadTail(const std::vector<cv::Point>& wormBdry, int& loc1, int& loc2, double mini_dist = 0.3);
	void getCandidateHeadTail(const std::vector<cv::Point>& wormBdry, std::pair<int, double>& loc_ang_pr1, std::pair<int, double>& loc_ang_pr2, double mini_dist);
	void flipWormGeometry(); /// Flip head and tail and all member data

	///Get drosal/ventral boundary - NOT ventral/dorsal disambiguated.
	void WormShape::splitContour(const std::vector<cv::Point>& wholeBry, int& loc1, int& loc2, std::vector<cv::Point>& DrlBdry, std::vector<cv::Point>& VtrlBdry);
	void getCoordMask(std::vector<cv::Mat>& Masks, cv::Size imgsz, const std::vector<cv::Point>& Bdry1, const std::vector<cv::Point>& Bdry2, double width = 1);
	void getRegionMask(cv::Mat& Mask, cv::Size imgsz, const std::vector<cv::Point>& Bdry1, const std::vector<cv::Point>& Bdry2, double start_loc, double end_loc, double width = 1);
	void getBinMeanInten(std::vector<double>& outvec, const cv::Mat& inputary, const std::vector<cv::Mat>& Masks);
	void getCenterline(std::vector<cv::Point>& ctrline, const std::vector<cv::Point>& Bdry1, const std::vector<cv::Point>& Bdry2);
	void getSkeletonMovement(const std::vector<cv::Point>& ctrline1, const std::vector<cv::Point>& ctrline2, double& movement, double start_loc, double end_loc, std::vector<cv::Point>& moving_p1, std::vector<cv::Point>& moving_p2);
	bool checkHeadTail(std::vector<double> body_intensity);
	bool checkHeadTailCorr(const std::vector<double>& templates, const std::vector<double>& body_inten);

	std::vector<double> bf_profile, fluo_profile;
	std::deque<std::vector<double>> bf_profile_history;


	/// Draw the current WormShape's segmentation on an image. 
	/// Set debug to true to display the image right away
	void drawSegmentation(cv::Mat& img_color, bool debug = false);

	/*
		Track a worm using flowPass
	*/
	// tracks a worms contour based on surounding pixel intensities
	// returns the worms next contour
	// @throws "worm is lost"
	static std::vector<cv::Point> flowPass(const cv::Mat &frame, std::vector<cv::Point> last_contour);




	/// Possibly deprecated - worm sex likelihood from Andrew


	/*
		DEPRECATED - REMOVE?
	*/

	// update the WormShape geometry from the given contour
	void WormShape::updateContour(const std::vector<cv::Point> &_contour);
	void WormShape::updateContourV2(const std::vector<cv::Point> &_contour); // Test version 

	// reset the WormShape geometry
	// initial area and time tracked are reset
	void WormShape::reset(const std::vector<cv::Point> &_contour);

	//Reset the centroid
	void WormShape::reset_centroid(cv::Point& centroid_assign);

	// Reset the ROI
	void WormShape::reset_roi(cv::Rect& roi_assign);

	// reset initial area and time tracked but keep the contour
	void WormShape::reset();

	// set the WormShape to empty
	void WormShape::clear();

	// update the sex of the worm
	void WormShape::updateSex(int step);

	// get the sex of the worm
	float WormShape::getSex() const;

	// returns if the WormShape is empty
	bool isEmpty() const;

	// returns if the centroid of WormShape is empty
	bool isEmpty_centroid() const;

	// returns if the roi of Wormshape is empty
	bool WormShape::isEmpty_roi() const;

	//returns the distance travelled by the worm
	float getDistanceTraveled() const;

	// returns how long the worm has been tracked for
	double getTimeTracked();

	//returns the worms centroid
	cv::Point getWormCentroid() const;

	//returns the roi of tracked worm
	cv::Rect getWormRoi() const;

	// returns the worms contour
	std::vector<cv::Point> getContour() const;

	// returns the initial area of the worm from the first contour
	float WormShape::getInitialArea() const;

	// returns the current area of the worm
	float WormShape::getArea();
	float WormShape::getAreaV2(); // Test function

	cv::Rect WormShape::boundingBox() const;

	float WormShape::getDisplacement() const;

	// returns the intensity of pixels in a box around the worm
	// used for calculating how isolated a worm is
	// region_size - the fraction of the frame that is used (0.0 - 1.0)
	float avgSurroundingIntensity(const cv::Mat &img, float region_size = 0.16);


	//returns a matrix containing a skeleton of the worm
	// TODO - PRUNE
	cv::Mat getSkeleton();

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

	/// Worm CNN for high magnification
	static cv::dnn::Net picker_net_hi_mag;

	// Worm speed and location (from main camera)
	cv::Point centroid;		/// Location in the current field of view
	cv::Point centroid_global; /// Location relative to whole-plate coordinate







	///////////////////////        Deprecated - delete later?         /////////////////////
	float distanceTraveled;
	AnthonysTimer trackTimer;

	cv::Rect roi; // ROI of tracking region after a worm has been targeted
	boost::optional<float> area;
	float initial_area;
	float initial_sex;
	float sex;
	cv::Point initial_position;

	
	void WormShape::thinning(cv::InputArray input, cv::OutputArray output);
	static void WormShape::thinningIteration(cv::Mat img, int iter);
};

#endif
