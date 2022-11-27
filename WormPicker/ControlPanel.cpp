/*
	ControlPanel.cpp
	Zihao Li
	Fang-Yen Lab
	June 2020

	ControlPanel: Class that holds parameters for searching or/and tracking worms using energy functional
	Setter and getter functions included in this file

*/

#include "ControlPanel.h"
#include <iostream>

ControlPanel::ControlPanel() {

	// Parameters for CNN
	dsfactor = 0.4;
	conf_thresh = 6.0;
	norm_type = 1;

	// Parameters for energy functional
	// Prototype energy functional:
	// Energy = alpha * Distance to pickup Loc + beta * pixel change + gamma * Confidence
	alpha = 0;
	beta = 1;
	gamma = 1;
	energy_thresh = 5000;

	// Parameters for worm searching/tracking setting
	update_ctr_rois = false;
	max_trans_dis = 10;

}


ControlPanel::ControlPanel(cv::Size sz, std::string fonnx_worm) {

	// Parameters for CNN
	if (!fonnx_worm.empty()) { net = cv::dnn::readNetFromONNX(fonnx_worm); } // CNN network
	dsfactor = 0.4;
	conf_thresh = 6.0;
	norm_type = 1;

	// Parameters for energy functional
	// Prototype energy functional:
	// Energy = alpha * Distance to pickup Loc + beta * (pixel change - mean) / SD + gamma * (Confidence - mean) / SD + delta * Intensity classification score
	alpha = 0;
	beta = 1; miu_pix = 400; sd_pix = 300; //3795// miu_pix = 2000; sd_pix = 2000; // miu value of 3795 will require a pixel difference of at least 15 pixels before a movement hotspot would pass the movement energy threshold
	gamma = 1; miu_conf = 5.8; sd_conf = 1.7; //5.8
	delta = 0; inten_thresh = 70;
	energy_thresh = 0;
	ergy_thres_cnn = 0;
	ergy_thres_motion = 0.1;
	thres_diff_img = 25; // thres_diff_img value changed after inspecting background pixel intensities in Matlab, found they were < 10.
	get_bin_mot = true;
	ergy_thres_inten = 0;
	get_bin_class = false;
	blursize = cv::Size(15, 15);
	smallsizetoremove = cv::Size(15, 15);

	sz_full_img = sz;

	// Parameters for worm searching/tracking setting
	search_ROI.x = 0;
	search_ROI.y = 0;
	search_ROI.width = sz.width;
	search_ROI.height = sz.height;

	ROIs.push_back(search_ROI);

	rad_roi = 40; // Radius of ROI
	//std::vector<cv::Point> ctr_rois; // Vector that contains center of ROIs at current frame
	update_ctr_rois = false;
	max_trans_dis = 10;
	bin_amount = 4;
	single_worm = true;

	// Default Representation mode
	Rep_idx = 0;

	// Lower energy threshold for worm tracking in a small roi
	ergy_thres_mon = gamma * ( 4 - miu_conf) / sd_conf;
}
