/*
	ControlPanel.h
	Zihao Li
	Fang-Yen Lab
	June 2020

	Class that holds parameters for searching or/and tracking worms using energy functional

*/

#pragma once

#ifndef CONTROLPANEL_H
#define CONTROLPANEL_H
#define SEARCH 1
#define MONITOR 2
#define SINGLE 1
#define MULTIPLE 0
#define ALL -1
#include "opencv2\opencv.hpp"
#include <opencv2/dnn/dnn.hpp>


class ControlPanel
{
public:
	// Constructer
	ControlPanel();
	ControlPanel(cv::Size sz, std::string fonnx_worm);

	// Representation mode
	enum Repres_mode { MULT, SIN };
	int Rep_idx; // 0: Multiple Representation (Find all the worms in ROI)
				 // 1: Single Representation (Track one worm in ROI)

	double ergy_thres_mon; // Lower energy threshold for worm tracking in a small roi

	// Parameters for CNN
	cv::dnn::Net net; // CNN network
	double dsfactor; // Downsample factor applied to image before feeding to CNN
	double conf_thresh; // Confidence threshold
	int norm_type; // Normalized type

	// Parameters for energy functional
	// Prototype energy functional:
	// Energy = alpha * Distance to pickup Loc + beta * (pixel change - mean) / SD + gamma * (Confidence - mean) / SD + delta * Intensity classification score
	// To do: update the def of energy in all the threads
	double alpha;
	double beta, miu_pix, sd_pix;
	double gamma, miu_conf, sd_conf;
	double delta; int inten_thresh;
	double energy_thresh; // Threshold for total energy value
	double ergy_thres_cnn; // Threshold for CNN energy value
	double ergy_thres_motion;// Threshold for motion energy value
	int thres_diff_img; // Threshold apply to raw difference image (remove noise). If negative, the thresholding will not be applied to diff image
	bool get_bin_mot;// Binary indicator that determine whether get the pixel difference of bins
					   // false: output the pixel difference image and will not calculate pixel difference of bins (assign all 0 to all bins)
					   // true: output the pixel difference image and calculate pixel difference as well
	double ergy_thres_inten;// Threshold for intensity energy value
	bool get_bin_class;// Binary indicator that determine whether get the classification score of bins
					   // false: output the thresholded image and will not calculate classification score (assign all 0 to all bins)
					   // true: output the thresholded image and calculate classification score as well
	cv::Size blursize;
	cv::Size smallsizetoremove;
	int track_worm_option; // used to track worms
	int bin_amount; // amount of bins needed to be searched
	bool single_worm; // single worm or not

	// Parameters for worm searching/tracking setting
	cv::Size sz_full_img; // Size of full size image
	cv::Rect search_ROI; // Search ROI
	std::vector<cv::Rect> ROIs; // Vectors that contain multiple ROIs. This is what ParalWormFinder uses when it runs
	std::vector<cv::Rect> custom_finding_rois; // A custom vector of ROIs in which to find all worms, if we want to only look in
											   // a subset of the whole image for worms.
	int rad_roi; // Radius of tracking ROI
	std::vector<cv::Point> ctr_rois; // Vector that contains center of ROIs at current frame
	bool update_ctr_rois; // Binary variable that indicate whether update center of ROIs for next frame
						  // 0: Not update center of ROIs (search/track worm in fix ROIs); 1: Update center
						  // 1: Update center of ROIs (track worm in moving ROIs)
	int max_trans_dis; // Maximum translation distance for center of ROI between two consecutive frames

	int rad_bin_cnn = 10; // Radius of bin to crop the downsampled ROI image, fed to CNN
	int rad_bin_motion = 10; // Radius of bin to crop the RAW ROI image, fed to motion detector, cannot be larger than rad_bin_cnn / dsfactor
	int rad_bin_thres = 10; // Radius of bin to crop the RAW ROI image, fed to intensity thresholder, cannot be larger than rad_bin_cnn / dsfactor



	// Setter functions that adjust the parameters
	void ResetCtrlPanel(); // Reset parameters to default setting


};

#endif
