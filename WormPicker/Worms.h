/*
	SortedBlobs.h
	Anthony Fouad
	Fang-Yen Lab
	February 2018

	Holds segmented blobs matching some parameters
*/

#pragma once
#ifndef SORTEDBLOBS_H
#define SORTEDBLOBS_H


// OpenCV includes
#include "opencv2\opencv.hpp"



// Standard includes
#include <cstdio>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>

// Anthony's includes
#include "AnthonysColors.h"
#include "AnthonysTimer.h"	
#include "HardwareSelector.h"
#include "WormShape.h"
#include "AnthonysCalculations.h"
#include "ThreadFuncs.h"
#include "StrictLock.h"
#include "ControlPanel.h"

// Boost includes
#include "boost\thread.hpp"
#include "boost\thread\lockable_adapter.hpp"

enum pick_mode {AUTO, MANUAL, DROP, NONE};
enum is_watching_for_picked{ NOT_WATCHING, START_WATCHING, WATCHING };


class Worms : public boost::basic_lockable_adapter<boost::mutex>
{

	// SortedBlobs is a friend of WormPick, allowing WormPick access to the pickup_location variable
	friend class WormPick;

public:


	// Constructor
	Worms(cv::Size sz, int& wat, std::string fonnx_worm);

	// Destructor
	~Worms();


	// Load parameters from text file
	void loadParameters();

	//Write parameters to text file
	void writeParameters();

	//rough pass for worm select mode
	int Worms::roughPass();

	// Extract blobs matching this set of parameters from the image
	int extractBlobs();

	// Evaluate all blobs to check if they are likely real worms
	void evaluateBlobs(const cv::Mat &img);

	// Evaluate blobs in high-mag image (Prototype)
	void evaluateHiMagBlobs();

	// Track worms using CNN in a masked region
	void TrackWormCnn(const cv::Mat& imggray, const cv::dnn::Net& net);

	//set tracked worm to a worm near the point (x, y)
	bool findOnClick(int x, int y);

	//select a worm to track 
	bool chooseWorm(pick_mode mode);

	// Update deque of frame
	void UpdateDeque(cv::Mat& thisframe);

	//Test whether the object in ROI near the centroid generated by CNN is moving
	//by measuring the pixel difference over a time window, e.g. 5s
	//void isMoving();

	// Get the absolute pixel differences in ROIs between two frames
	std::vector<int> RoiPixelDiff(cv::Mat& Differeence_Img, std::vector<cv::Point> vec_point);

	// Measure the absolute pixel differences between two frames for each bin in ROI
	std::vector<int> measureBinPixDiff(std::vector<cv::Point>& coords, cv::Mat& Pixel_change, int bin_radius, bool get_bin_mot);

	// Measure the classification score by intensity for each bin in ROI
	std::vector<double> measureIntenBinClass(std::vector<cv::Point>& coords, cv::Mat& Roi_img, int bin_radius, int thresh, std::vector<cv::Mat>& bw, cv::Size blursz, cv::Size removesz, bool get_bin_class);

	//update the tracking
	void updateTrack(bool wormSelectMode);

	// Find worm closest to the pickup point
	bool findWormNearestPickupLoc();

	// Find worm closest to the pickup point (Using the centroids generated by CNN)
	bool findWormNearestPickupLocCNN();

	// Get the worm closest to the pickup point from CNN, i.e. TrackWormCnn
	bool getWormNearestPickupLocCNN();

	// clear the tracked contour
	void resetTrack();

	//verify that the worm was picked either looking for the worm or looking for diff img movement
	bool wasWormPickedByBlob();
	bool mvmtAtPickedSite();

	//verify that the worm was picked by tracking the worm near pick-up site in next few seconds
	double WormMvmFromPickedSite();

	//verify that the worm was dropped
	bool wasWormDropped();

	void setPickupLocation(int x, int y);

	void Worms::setPickedWorm();

	void disableTracking();

	void enableTracking();

	// Assign value to worms' centroid
	void assigncentroidAll(std::vector<cv::Point> coords);

	// Assign pixel changes in ROIs of worms
	void assignPixDiffRoi(std::vector<int> vec_PixDiff);

	// Update the centroid energy
	void UpdateEnergyCentroid(std::vector<double> vec_energy);

	// Update centroids' distance to pick-up location
	void UpdateCentroidDist();

	// Update centroids' moved distance since last frame
	void UpdateMvDist();

	// Update centroids' confidence of single Frame CNN
	void UpdateConfiSinglefrmCnn(std::vector<double> vec_conf);

	// Update high energy centroid
	void UpdateHighEnergyCentroids(std::vector<cv::Point> vec_centroid);

	// Update monitored ROIs
	void UpdateMonitorRois(std::vector<cv::Rect> vec_rois);

	// Update Coordinate offset
	void UpdatePixOffset(cv::Point2d pt1, cv::Point2d pt2, double dydOx);

	// Update contours of worms
	void UpdatecontoursAll(std::vector<std::vector<cv::Point>>& cturs);

	// Update Area of worms
	void UpdateAreaAll();

	// Calculate the energy functional of centroids and select a centroid of highest energy
	std::pair<cv::Point, int> CalCentroidEnergy(double alpha, double beta, double gamma);

	// Set searching Roi;
	void setSearchRoi(cv::Point center, int radius);
	void resetSearchRoi();// Reset the searching ROI back to the whole image

	// Remove the centroid points that have low energy
	void GetHighEnergyCentroid(double ener_thresh);


	// Calculate the energy functional that reveal the possibility of centroids to be worms
	// Return the point that is of lowest energy value
	void FindWormEnergy(const cv::dnn::Net& SingleFrameNet, const cv::dnn::Net& MultiFrameNet);
	// TO DO

	// Worm-finding/tracking module that allow CNN, Motion detector and intensity classifier process in parallel
	// CtrlPanel: parameter holder; worm_posit: output vector holding worm positions; vec_ergy: output vector holding worm energy
	void ParalWormFinder(const cv::Mat& cur_frm, const cv::Mat& pre_frm, const ControlPanel CtrlPanel, std::vector<cv::Point>& worm_posit, std::vector<double>& vec_ergy, std::vector<cv::Mat>& diff_img, std::vector<cv::Mat>& thres_img);

	// Dummy worm finder/tracker using saved video file or frames
	std::vector<std::vector<cv::Point>> WormTrajectory(std::vector<cv::Mat>& vec_frms, const std::vector<cv::Point> initial_pts);
	
	std::vector<cv::Point> DmyTrajTracker(const ControlPanel& CtrlPanel, const cv::Mat& frm);
	void setDmyTrajTracker(ControlPanel& CP, std::vector<cv::Point> last_pts);


	// Worm-finding/tracking threads using ParalWormFinder for main and fluorescence cameras
	void MainCamWormFinder(cv::Size sz, std::string fonnx_worm);
	void FluoCamWormFinder(cv::Size sz, std::string fonnx_worm_hi_mag);

	// Basic functions for specific tasks in worm finder
	// Main Camera
		void MainCamFindWormRoi(const ControlPanel CtrlPanel); // Get the worm position in the whole FOV of main cam, use default parameter object
			// Delete but merge with ParalWormFinder
		void MainCamTrackWormRoi(const ControlPanel CtrlPanel); // Track worms in a small ROIs
			// Delete but add otpion in control panel to specify if tracking is desired, and if so ParalWormFinder
			// Does the same tracking code

		void MainCamSegmWormRoi(const ControlPanel CtrlPanel); // Segment worm contours in small ROIs
			// Delete but if functionality is needed elsewhere can use of segmentWorm()

		void MainCamFindWormContr(const ControlPanel CtrlPanel); // Get the worm contour using thresholding in the overlap FOV with high mag camera
			// Use segmentWorm for this

		
		

	// Basic parameter object creater complementary to basic functions above
	void setMainCamTrackWormRoi(ControlPanel& CP);
	void setMainCamSegmWormRoi(ControlPanel& CP, std::vector<cv::Rect> ROIs);
	void setMainCamFindWormContr(ControlPanel& CP);
	void setMainCamFindWormNearPickupLoc(ControlPanel& CP, int rad);

	void setFluoCamSegWormWholeFOV(ControlPanel& CP);
	void setFluoCamGetFluoImg(ControlPanel& CP);



	void SearchWormWholeImg(const cv::dnn::Net SingleFrameNet, const cv::dnn::Net MultiFrameNet, cv::Mat& imggray, cv::Mat& DiffImg, double dsfactor, double conf_thresh, double energy_thresh, int norm_type, double alpha, double beta, double gamma);

	void SearchWormNearPickupLoc(const cv::dnn::Net SingleFrameNet, const cv::dnn::Net MultiFrameNet, cv::Mat& imggray, cv::Mat& DiffImg, double dsfactor, double conf_thresh, double energy_thresh, int norm_type, double alpha, double beta, double gamma);

	void SearchWorm(const cv::dnn::Net SingleFrameNet, const cv::dnn::Net MultiFrameNet, cv::Mat& imggray, cv::Mat& DiffImg, double dsfactor, double conf_thresh, double energy_thresh, int norm_type, double alpha, double beta, double gamma);

	void SearchWorm(const ControlPanel CtrlPanel);

	// Monitor worms in ROIs
	void MonitorWormROIs( std::vector<cv::Point>& centers, int roi_radius, cv::Mat& imggray, cv::Mat& DiffImg, cv::dnn::Net graynet, double dsfactor, double conf_thresh, int norm_type, double max_dist, double alpha, double beta, double gamma);

	void MonitorWormROIs(const ControlPanel CtrlPanel);

	// Monitor worms near the pick-up location
	void MonitorWormPickupLoc(int roi_radius, cv::Mat& imggray, cv::Mat& DiffImg, cv::dnn::Net graynet, double dsfactor, double conf_thresh, int norm_type, double alpha, double beta, double gamma);

	// Monitor worms using Gaussian weight distribution
	void MonitorWormRoiGaus(std::vector<cv::Point>& centers, int roi_radius, cv::Mat& imggray, cv::Mat& DiffImg, cv::dnn::Net graynet, double dsfactor, double conf_thresh, int norm_type, double max_dist, double alpha, double beta, double gamma);

	// Print out the movement of centroids
	void PrintCentroidMovement() const;


	//combine searchWorm and MonitorWormROI
	void SearchOrMonitorWorms(const ControlPanel CtrlPanel);


	//a better search worm function

	void SearchWormsAmount(const ControlPanel CtrlPanel);

	//choose best worm with highest energy

	void ChooseOneWorm(const ControlPanel CtrlPanel);


	// Measure metrics of head and tail for classification purpose // TO DO: add controlpanel to be input argument
	void AssessHeadTail(const ControlPanel& CtrlPanel, const cv::Mat& cur_frm, const cv::Mat& pre_frm, std::pair<double, double>& angles, std::pair<double, double>& movement, std::pair<double, double>& intensity, std::vector<double>& body_inten, double length, double width);


	// Verify the pick-up or unloading success
	bool WrmCrwlOutOfPk();


	// GETTER FUNCS

	WormShape getTrackedWorm() const;
	WormShape getHiMagWorm() const;
	cv::Point getTrackedCentroid() const;
	cv::Rect Worms::getTrackedRoi() const;
	std::vector<cv::Rect> Worms::getMonitorRoi() const;
	std::vector<cv::Point> getTrackedContour() const;
	std::vector<std::vector<cv::Point>> getWormContours() const;
	std::vector<cv::Point> getcentroidAll() const;
	std::vector<double> getareaAll() const;
	std::vector<int> getPixelDiff() const;
	std::vector<double> getConfiSinglefrmCnn() const;
	std::vector<double> getCurvatures(std::vector <cv::Point > input) const;

	// std::vector<double> SortedBlobs::getAngles(std::vector<cv::Point> input) const;


	cv::Point getcentroidbyidx(int idx) const;
	int getNumCentroid() const;
	int getNumBlobs() const;
	double getDistToPickup() const;
	cv::Point getPickupLocation() const;
	cv::Point getCtrOvlpFov() const; // Get the center of overlap FOV
	bool isWormOnPickup() const;
	bool isWormOnPickupCNN() const;
	bool Worms::isWormTracked() const;

	//TODO PUBLIC VARIABLES - CHANGE ALL TO PROTECTED LATER ///////////////////////////////////////////////////////

	// Binary image for storing the blobs of the worms that match this set of parameters
	cv::Mat		bwcontours;
	cv::Mat		imggray;
	cv::Mat		img_high_mag; // High magnification image from thorlabs camera
	cv::Mat		Diff_img; // Difference image
	cv::Mat		pre_frame_hi_mag; // Precedent high-mag frame
	cv::Mat		pre_frame_lo_mag; // Precedent low-mag frame


	cv::Rect	ovlpFov = cv::Rect(cv::Point(250, 193), cv::Point(334, 263)); // FOV of main camera overlapped with fluo camera

	std::vector<cv::Mat> ImageQue;

	// Information about the number of blobs and their quality
	std::vector<double> evalmetrics;
	int thresh;

	// Overlay flag
	bool overlayflag;	
	
	bool wormSelectMode;


	// ControlPanel object that holds parameters for worm-finding and tracking thread
	ControlPanel ctrlPanel;

	// Binary variables that indicate the worm-finding status
	bool findWorminWholeImg;
	bool findWormNearPickupLoc;
	bool trackwormNearPcikupLoc;
	bool trackMultipleWorm;
	bool limit_max_dist; // limit the maximum movement
	int worm_finding_mode; // 0: Search worm in whole image; 1: Track worm in ROI; 2: Search worm near pick-up location; --- This should probably be an enum

	cv::Rect search_roi; // Searching Roi for finding worms

	double max_move;

	// tracking mode
	bool trackbyCtrofMass;

	
	const double track_radius = 55;
	const double max_dist_to_pickup = track_radius / 6;

	const int track_mvm_radius = 25;

	int pickup_radius = 60;

	cv::Rect pickup_Roi;

	// the minimum amout of movement necessary for the contour to be deemed a worm
	// over a period of length track_time (5 seconds) using the blob method
	const int movement_threshold = 20; //todo calibrate

	int &watching_for_picked;	// whether we are currently watching (e.g. a difference image) to see if a worm was picked
	double diff_search_ri, diff_search_ro;

	// the time spent tracking worms before they are chosen
	const int track_time = 5;

	std::vector<std::vector<cv::Point>> contoursAll;

	std::vector<cv::Point> centroidAll;

	std::vector<double> areaAll;

	std::vector<int> pixel_change_rois; // pixel changes in ROIs of worms

	double contrast_max;

	cv::Point max_prob_worm_centroid;

	cv::Point max_energy_centroid;

	std::vector<double> DistCentroidtoPickupLoc;

	std::vector<double> MovedDistance;

	std::vector<double> ConfiCentroid_singlefrmCnn;

	std::vector<double> ConfiCentroid_multifrmCnn;

	std::vector<double> EnergyofCentroids;

	std::vector<cv::Point> HighEnergyCentroids;
	
	std::vector<cv::Rect> Monitored_ROIs;

	cv::Point2d Current_coord; // Current stage coordinate
	cv::Point2d last_coordinate;
	cv::Point2d offset;

	cv::Rect unload_search_Roi; // Searching region for verifying the success of unloading


	std::vector<cv::Point> origin_traj;

	// refactor
protected:

	// Mutex for internal locking
	boost::mutex mtx_; /// explicit mutex declaration 

	// track a worms contour based on centroids
	// returns the worms next contour
	// @throws "worm is lost"
	std::vector<cv::Point> centroidTrack(const WormShape &last_worm, int search_radius);


	//checks if the worm is unique to all the worms in worm_list
	//determined by comparing centroids
	static bool isWormNew(const WormShape &worm, const std::vector<WormShape> &worm_list, int search_radius);

	std::vector<WormShape> all_worms;
	WormShape picked_worm;
	WormShape tracked_worm;
	WormShape hiMag_worm; // Worm under high magnification camera
	cv::Point pickup_location;
	cv::Point fluo_cam_location;
	bool trackingEnabled;
	
	// Threshold values for worms in this class for area, "Length" (perim/2), and L/W ratio (perim/2) 
	double area_min, area_max, length_min, length_max, lwratio_min, lwratio_max;
};

#endif