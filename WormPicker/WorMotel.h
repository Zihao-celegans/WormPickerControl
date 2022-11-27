
/*

	WorMotel.h
	Zihao Li
	Fang-Yen Lab
	November 2022

	Object containing infomation of about the WorMotel

*/


#pragma once

#ifndef WORMOTEL_H
#define WORMOTEL_H


// OpenCV includes
#include "opencv2\opencv.hpp"

// Zihao's includes


class WorMotel
{

public:

	WorMotel::WorMotel();

	// Get moat cross template for pattern matching
	void getMoatCrossTemplate();

	// Detect moat cross coordinates using pattern matching method
	void DetectMoatCrossCoord(std::string img_path, std::string save_path, cv::TemplateMatchModes MatchMode, bool show_debug = false);
	

private:

	// image path for getting moat cross template
	std::string img_path_to_get_temp;

	// Template for moat cross
	cv::Mat CrossTemplate;

	// down sample factor for improving computation speed
	double dsfactor;

	// Params for cropping template
	// x and y coordinate of upper left corner in terms of percentage of image width and height
	double p_x;
	double p_y;
	// width and height in terms of percentage of image width and height
	double p_width;
	double p_height;

	// threshold for template matching result (0 ~ 255)
	int temp_match_thresh;

	// Params of intensity filtering
	double inten_range_factor; // determine the range for accepting moat cross intensity, in terms of standard deviation

	double min_inten_moat_cross;
	double max_inten_moat_cross;
	double mean_inten_moat_cross;
	double std_inten_moat_cross;

	double low_inten_thresh; // low inten threshold = mean - inten_range_factor * std
	double high_inten_thresh; // high inten threshold = mean + inten_range_factor * std

	
	// Coordinates of identified moat cross

	// Coordinates of identified well center
	



};

#endif