/*
	WPHelper.h
	Anthony Fouad
	July 2019

	Helper functions for wormpicker that were formerly located in ImageFuncs.cpp. ImageFuncs.cpp now has only functions
	shared between wormpicker and wormwatcher, not functions specific to wormpicker
*/
#pragma once
#ifndef WPHELPER_H
#define WPHELPER_H

// OpenCV includes
#include "opencv2\opencv.hpp"


// Standard includes
#include <cstdio>
#include <string>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <vector>

// Anthony's includes
#include "stdafx.h"
#include "AnthonysColors.h"
#include "WormFinder.h"
#include "AnthonysCalculations.h"
#include "OxCnc.h"
#include "HardwareSelector.h"

// Forward declaration of WormPick
class WormPick;

// Basic image segmentation
void segment_image(int thresh, const cv::Mat &imgin, cv::Mat &bwout, std::vector<std::vector<cv::Point>> &contoursout, HardwareSelector &use);
void DNN_segment_worm(int thresh, const cv::Mat &imgin, cv::Mat &bwout, std::vector<std::vector<cv::Point>> &contoursout, HardwareSelector &use, cv::dnn::Net net_worm);

double threshold_with_mask(const cv::Mat &src, cv::Mat &dst, double thresh, double maxval, int type, const cv::Mat &mask = cv::Mat());
double otsu_8u_with_mask(const cv::Mat src, const cv::Mat &mask);

// Display the annotated and segmented images
void display_image(int& k, cv::Mat &imgcolorin, cv::Mat &imgcolorin_tl, const cv::Mat &bwin, const cv::Mat &bwin_tl, const WormPick &Pk,const WormFinder& worms,
	const std::string &message = "", const bool &wormSelectMode = false);

void display_image(int& k, cv::Mat &imgcolorin, const cv::Mat &bwin, const WormFinder& worms,
	const std::string &message = "", const bool &wormSelectMode = false);

void display_image(int& k, cv::Mat &imgcolorin, cv::Mat &imgcolorin_tl, const cv::Mat &bwin, const WormFinder& worms,
	const std::string &message = "", const bool &wormSelectMode = false);



void displayImageThreadFunc(int& k, cv::Mat &imgcolorin, cv::Mat &imgcolorin_tl, const cv::Mat &bwin, const cv::Mat &bwin_tl, const WormPick &Pk, const WormFinder& worms,
	const std::string &message = "", const bool &wormSelectMode = false);

// Determine which threshold to use automatically
int optimize_threshold(const cv::Mat &img, WormFinder& worms, HardwareSelector &use, ErrorNotifier &Err);


// Save image metadata to disk
void writeMetaData(double frametime, std::ofstream& out, const WormFinder& worms, const OxCnc& Ox, const OxCnc& Grbl2, const WormPick& Pk);

// Compute TENV (Tenengrad variance) focus metric for an image
	/// Use resize_factor [0,1) to downsample the image and speed up the calculation
	/// Alternatively, use overload to speed up calculation by focusing on area near pick tip.
double tenengrad(const cv::Mat& src, int ksize = 3, double resize_factor = 1);
double tenengrad(const cv::Mat& src, cv::Point pt, int radius);

// Automatically focus the image
void focus_image(OxCnc& Ox, WormPick& Pk, WormFinder& worms, int isLowmag);

// Mean value of an image (same ROI setup as tenengrad)
double localIntensity(const cv::Mat& src, cv::Point pt, int radius);



void CallBackFunc(int event, int x, int y, int flags, void* userdata);

#endif
