/*
	ImageFuncs.h
	Anthony Fouad
	Fang-Yen Lab
	Updated July 2019

	Functions for basic acquisition, segmentation of images shared between WormPicker and WormWatcher
	NO FUNCTIONS THAT USE WORMPICKER SPECIFIC CLASSES
*/

#pragma once
#ifndef IMAGEFUNCS_H
#define IMAGEFUNCS_H

// OpenCV includes
#include "opencv2\opencv.hpp"
#include <opencv2/dnn/dnn.hpp> 


// Standard includes
#include <cstdio>
#include <string>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <vector>

// Anthony's includes
#include "AnthonysColors.h"
#include "AnthonysCalculations.h"
#include "HardwareSelector.h"

// Callback function for mouse clicks
void CallBackFunc(int event, int x, int y, int flags, void* userdata);

// Anthony's OpenCV supplement: Draw a cross
void drawCross(cv::Mat &img, cv::Point ctr, cv::Scalar color, int thickness, int radius);

void drawCrossesForPoints(cv::Mat &img, std::vector<cv::Point> ctrs, cv::Scalar color, int thickness, int radius);

// Anthony's OpenCV supplement: Draw a trace from a vector of Point's. Minimum 2 points. 
void drawPlot(cv::Mat &img, std::vector<cv::Point> pts, cv::Scalar color, int thickness = 1, bool closed = false);

// Correlation between two CV_32F Mat's
double correlation(cv::Mat img1, cv::Mat img2);

// Anthony's OpenCV supplement: Wait for a key press(es) that is (are) a valid N digit number
int waitKeyNum(int N, int wait_time, cv::Mat img_display, cv::Point num_loc, std::string fig_name);
int vectorNum(std::vector<int> v);
int filterObjects(const cv::Mat& bwin, cv::Mat& bwout, int min_size, int max_size);
int filterObjectsLWRatio(const cv::Mat& bwin, cv::Mat& bwout, double LWR_min, double LWR_max, double L_min, double L_max);
cv::Mat gaussFiltExclZeros(cv::Mat U, int ksize, double sig);
void filterObjectsTouchingBorder(const cv::Mat& bwin, cv::Mat& bwout);
bool contourTouchesImageBorder(std::vector<cv::Point>& contour, cv::Size& imageSize);
std::string cvtype2str(int type);
cv::Mat rotateImage(const cv::Mat source, double angle, int border = 0);
cv::Point centroidOfPoints(std::vector<cv::Point> pts);
void centroidOfContours(std::vector<std::vector<cv::Point>> contours, std::vector<cv::Point>& out_centroids);
cv::Mat meanOfImages(const std::deque<cv::Mat>& images, int i0 = 0, int i1 = 999999, int skip = 1);
cv::Mat minOfImages(const std::deque<cv::Mat>& images, int i0 = 0, int i1 = 999999, int skip = 1);

void segmentDarkestN(const cv::Mat& imgin, cv::Mat &imgout, int N, cv::Mat mask = cv::Mat::zeros(cv::Size(1, 1), CV_8U));

void isolateLargestBlob(cv::Mat &bw, int erode_size = 3);
std::vector<cv::Point> isolateLargestContour(const cv::Mat& bw, int min_contour_size = 1);
void findoverlapBlob(cv::Mat &ref_bw, cv::Mat &obj_bw); // find the blobs in obj_bw that have overlap with blobs in ref_bw


// Geometry, related to worm contour / centelrine but general purpose
bool ResampleCurve(std::vector<cv::Point> original, std::vector<cv::Point>& resampled, int N);

// Measure convexity of a curve
double getConvexity(const std::vector<double>& vec_in, double POI_start, double POI_end);

int getPeakOrCenterOfMassOfIntensityDistribution(cv::Mat& img, bool want_Peak);
int getCliffOfIntensityDistribution(cv::Mat& img);


void getROIsfromPts(std::vector<cv::Point>& pts, cv::Size full_sz, std::vector<cv::Rect>& vec_Rois, int roi_radius);

// Helper functionns for finding WorMotel well

// Find group of points standing on a same vertical / horizontal line.
void getPointsAtSameVecHorztLine(std::vector<cv::Point>& Grid_pts, std::vector<std::vector<cv::Point>>& groups_of_pts, 
	cv::Size sz_img, double p_length_thresh, bool find_vertical);

// Linear extrapolation
void OneDimGridExtrapolate(std::vector<cv::Point>& in_points, std::vector<cv::Point>& out_points, bool extrap_in_x, int up_lim, int low_lim = 0);

void TwoDimGridExtrapolate(std::vector<cv::Point>& Grid_pts, std::vector<cv::Point>& extrap_Grid_pts, cv::Size sz_img, double p_leng_thresh);

#endif
