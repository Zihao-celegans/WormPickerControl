/*
	WormFinder.h

	Adapted from Worms.cpp/.h files originally implemented by Anthony Fouad
	The WormFinder.cpp/.h files are meant to replace the old Worms.cpp/.h files, which were overrun with deprecated and unused code.
	If a different file is looking in this file for a method that is not here (perhaps an old commented out or unused method needs to be in use again) then
	that method is most likely going to be in the Worms files and can be copied from there into here to remedy the problem.

	Peter Bowlin
	Fang-Yen Lab
	Janurary 2021

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
#include "Worm.h"
#include "AnthonysCalculations.h"
#include "ThreadFuncs.h"
#include "StrictLock.h"
#include "ControlPanel.h"

// Boost includes
#include "boost\thread.hpp"
#include "boost\thread\lockable_adapter.hpp"

enum pick_mode { AUTO, MANUAL, DROP, NONE };
enum is_watching_for_picked { NOT_WATCHING, START_WATCHING, WATCHING };
struct PickableRegion {
	cv::Point original_center; // The center of the pickable circle when we are centered on a plate.
	cv::Point pickable_center; // The current center of the pickable circle in the image (takes into account offset if we are not centered on the plate)
	int radius;
};



class WormFinder : public boost::basic_lockable_adapter<boost::mutex>
{

	// SortedBlobs is a friend of WormPick, allowing WormPick access to the pickup_location variable
	friend class WormPick;

public:
	
	// Constructor
	WormFinder(cv::Size sz, int& wat, std::string fonnx_worm, cv::Mat& full_res_bw_img, cv::Mat& high_mag_img, cv::Mat& img_high_mag_color, cv::Mat& img_low_mag_color);

	// Destructor
	~WormFinder();

	// Load parameters from text file
	void loadParameters();

	//Write parameters to text file
	void writeParameters();

	//rough pass for worm select mode
	int WormFinder::roughPass();

	// Extract blobs matching this set of parameters from the image
	int extractBlobs();

	// Evaluate all blobs to check if they are likely real worms
	void evaluateBlobs(const cv::Mat &img);

	//set tracked worm to a worm near the point (x, y)
	bool findOnClick(int x, int y);

	//User click on a position, move the high-mag camera to that point
	bool SetMoveOnClick(int x, int y);

	// Measure the absolute pixel differences between two frames for each bin in ROI
	std::vector<int> measureBinPixDiff(std::vector<cv::Point>& coords, cv::Mat& Pixel_change, int bin_radius, bool get_bin_mot);

	// Measure the classification score by intensity for each bin in ROI
	std::vector<double> measureIntenBinClass(std::vector<cv::Point>& coords, cv::Mat& Roi_img, int bin_radius, int thresh, std::vector<cv::Mat>& bw, cv::Size blursz, cv::Size removesz, bool get_bin_class);

	// Get the worm closest to the pickup point from CNN, i.e. TrackWormCnn
	bool getWormNearestPickupLocCNN();

	// clear the tracked contour
	void resetTrack();

	//verify that the worm was dropped
	bool wasWormDropped();

	void setPickupLocation(int x, int y);

	// Update monitored ROIs
	void UpdateMonitorRois(std::vector<cv::Rect> vec_rois);

	// Worm-finding/tracking module that allow CNN, Motion detector and intensity classifier process in parallel
	// CtrlPanel: parameter holder; worm_posit: output vector holding worm positions; vec_ergy: output vector holding worm energy
	//void ParalWormFinder(const cv::Mat& cur_frm, const cv::Mat& pre_frm, const ControlPanel CtrlPanel, std::vector<cv::Point>& worm_posit, std::vector<double>& vec_ergy, std::vector<cv::Mat>& diff_img, std::vector<cv::Mat>& thres_img);
	void ParalWormFinder(int cur_frm_idx, int pre_frm_idx, const std::vector<cv::Mat>& images, const ControlPanel CtrlPanel);
	void runWormFinder();
	void prepControlPanel(ControlPanel& CP);
	void getROIsfromPts(std::vector<cv::Point> pts, cv::Size full_sz, std::vector<cv::Rect>& vec_Rois, int roi_radius);

	void swapWormFindingMode(int mode, std::vector<cv::Rect>& ROIS); // TODO restrict to enum worm_find_options

	bool wormPickVerification();

	void drawAllWormPositions(cv::Mat& img_color, cv::Scalar color, bool display_image_after = true);
	void drawCirclesAtPoints(cv::Mat& img_color, std::vector<cv::Point> points, cv::Scalar color);

	std::vector<std::vector<cv::Point>> getWormTrajectories(std::vector<cv::Mat> trajectory_images);
//
//	std::vector<cv::Point> DmyTrajTracker(const ControlPanel& CtrlPanel, const cv::Mat& frm);
//	void setDmyTrajTracker(ControlPanel& CP, std::vector<cv::Point> last_pts);
//
//	// Worm-finding/tracking threads using ParalWormFinder for main and fluorescence cameras
//	void MainCamWormFinder(cv::Size sz, std::string fonnx_worm);
//
//	// Basic functions for specific tasks in worm finder
//	// Main Camera
//	void MainCamFindWormRoi(const ControlPanel CtrlPanel); // Get the worm position in the whole FOV of main cam, use default parameter object
//		// Delete but merge with ParalWormFinder
//	
//	void MainCamTrackWormRoi(const ControlPanel CtrlPanel); // Track worms in a small ROIs
//		// Delete but add otpion in control panel to specify if tracking is desired, and if so ParalWormFinder
//		// Does the same tracking code
//	
//
//// Basic parameter object creater complementary to basic functions above
//	void setMainCamTrackWormRoi(ControlPanel& CP);
//	void setMainCamSegmWormRoi(ControlPanel& CP, std::vector<cv::Rect> ROIs);
//	void setMainCamFindWormContr(ControlPanel& CP);
//	void setFluoCamSegWormWholeFOV(ControlPanel& CP);
//	void setMainCamFindWormNearPickupLoc(ControlPanel& CP, int rad);


//	void getROIsfromPts(std::vector<cv::Point> pts, cv::Size full_sz, std::vector<cv::Rect>& vec_Rois, int roi_radius);
	int getclosestPoint(std::vector<cv::Point> pts_list, cv::Point ref_pt);
	//void RemoveClosePts(std::vector<cv::Point>& worm_coords, std::vector<double>& prob_coords, double dist);
	void RemoveCloseWorms(double dist, std::vector<Worm>& removed_worms);
	void CalBinCoor(std::vector<cv::Point>& ds_coords, std::vector<cv::Point>& coords, cv::Size sz, double dsfactor);
	cv::Point2d CenterofMass(std::vector<cv::Point> vec_pts, std::vector<double> vec_probs);
	double totalmass(std::vector<double> vec_mass);

	int VerifyWormPickOrDrop(int num_expected_worms = 1, bool verify_drop_over_whole_plate = false);
	void getPickableWormIdxs(std::vector<int>& pickable_idxs);
	void updatePickableRegion(cv::Point3d current_pos, cv::Point3d plate_pos, double dydOx);
	int countHighMagFluoWorms(cv::Mat fluo_img, int thresh_val, int thresh_area, int fluo_spots_per_worm = 1);


	// GETTER FUNCS
	Worm* getTrackedWorm() const;
	Worm getHiMagWorm() const;
	cv::Point getTrackedCentroid() const;
	cv::Rect WormFinder::getTrackedRoi() const;
	std::vector<std::vector<cv::Point>> getWormContours() const;
	std::vector<int> getPixelDiff() const;
	std::vector<double> getConfiSinglefrmCnn() const;

	void setTrackedWorm(Worm* worm);

	// std::vector<double> SortedBlobs::getAngles(std::vector<cv::Point> input) const;


	int getNumBlobs() const;
	double getDistToPickup() const;
	cv::Point getPickupLocation() const;
	cv::Point getCtrOvlpFov() const; // Get the center of overlap FOV
	bool isWormOnPickup() const;
	bool isWormCentered(cv::Point target_loc = cv::Point(-1,-1), cv::Point worm_loc = cv::Point(-1,-1), double max_distance = -1) const;
	bool WormFinder::isWormTracked() const;
	void WormFinder::updateWormPosition(Worm& worm, cv::Point gbl_ctr_mass, double energy, int rad, bool is_lost);
	void WormFinder::updateWormsAfterStageMove(double dx, double dy, double dydOx);

	//TODO PUBLIC VARIABLES - CHANGE ALL TO PROTECTED LATER ///////////////////////////////////////////////////////

	// Binary image for storing the blobs of the worms that match this set of parameters
	cv::Mat		bwcontours;
	cv::Mat&	imggray;
	cv::Mat&	img_high_mag; // High magnification image from thorlabs camera
	cv::Mat&	img_high_mag_color;
	cv::Mat&	img_low_mag_color;
	cv::Mat		Diff_img; // Difference image
	cv::Mat		pre_frame_hi_mag; // Precedent high-mag frame
	cv::Mat		pre_frame_lo_mag; // Precedent low-mag frame
	
	cv::Mat worm_drop_verification;


	cv::Rect	ovlpFov = cv::Rect(cv::Point(250, 193), cv::Point(334, 263)); // FOV of main camera overlapped with fluo camera

	std::vector<cv::Mat> image_queue; // Vector that holds a set number of recent images. It is filled and maintained by the runWormFinder method.
	int most_recent_img_idx = 0; // Because the image queue cycles its images, this tracks the index of the image that was most recently added to the image queue

	// Information about the number of blobs and their quality
	std::vector<double> evalmetrics;
	int thresh;

	cv::Point high_mag_fov_center_loc;

	cv::Point loc_to_move_user_set;

	// Overlay flag
	bool overlayflag;
	bool laser_focus_display = false;
	double laser_focus_roi_w_mult = 0.24; // Fraction of the overall width of image (centered on the midpoint) that we will look at when focusing the laser
	double laser_focus_roi_h_mult = 0.03; // Fraction of the overall height of image (centered on the midpoint) that we will look at when focusing the laser
	int laser_focus_low_mag_offset; // Number of pixels in low mag cam to offset from the high mag FOV center to focus the image well (The laser was manually calibrated to the high mag FOV center, but it was not done perfectly so we determined this additional offset through testing) 
	int laser_focus_high_mag_offset;


	bool wormSelectMode;
	std::string recent_verification_technique;


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
	const double max_dist_to_pickup = track_radius / 11;

	const int track_mvm_radius = 25;

	int pickup_radius = 60;

	cv::Rect pickup_Roi;

	PickableRegion pickable_region;
	//cv::Rect pickable_region;
	bool verifying_pick_action = false; // True if we are currently attempting to verify a pick or dropping action
	int verify_radius_near_pick = 170; // The small radius around the drop off location to search for a recently dropped worm in verifying drop success
	int verify_radius_over_whole_plate; // The large radius over whole plate to search for a recently dropped worm in verifying drop success (useful for dropping worm to an empty plate)


	bool unknown_pheno_in_high_mag = false; // This tracks if we can't properly phenotype a worm in the high mag.

	// the minimum amout of movement necessary for the contour to be deemed a worm
	// over a period of length track_time (5 seconds) using the blob method
	const int movement_threshold = 20; //todo calibrate

	int &watching_for_picked;	// whether we are currently watching (e.g. a difference image) to see if a worm was picked
	double diff_search_ri, diff_search_ro;

	// the time spent tracking worms before they are chosen
	const int track_time = 5;

	std::vector<std::vector<cv::Point>> contoursAll;

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

	std::vector<Worm> all_worms;
	AnthonysTimer image_timer;
	enum worm_find_options { FIND_ALL, TRACK_ONE, TRACK_ALL, VERIFY, WAIT };
	enum fluo_type_options { NO_WORM, NON_FLUO_TYPE, FLUO_TYPE};
	enum sex_type_options { UNKNOWN_SEX, HERM, MALE };

	// refactor
protected:

	std::ofstream motion_data; // File for recording data

	// Mutex for internal locking
	boost::mutex mtx_; /// explicit mutex declaration 

	//checks if the worm is unique to all the worms in worm_list
	//determined by comparing centroids
	static bool isWormNew(const Worm &worm, const std::vector<Worm> &worm_list, int search_radius);

	Worm picked_worm;
	Worm*  tracked_worm = nullptr;
	Worm hiMag_worm; // Worm under high magnification camera
	cv::Point pickup_location;
	bool trackingEnabled;
	std::string fonnx_worm;
	cv::Size full_img_size;

	// Threshold values for worms in this class for area, "Length" (perim/2), and L/W ratio (perim/2) 
	double area_min, area_max, length_min, length_max, lwratio_min, lwratio_max;
};

#endif
