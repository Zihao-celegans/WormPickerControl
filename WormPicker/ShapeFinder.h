/*
	ShapeFinder.h
	Anthony Fouad
	Fang-Yen Lab
	Fall 2018

	Find specific types of shapes within an image.
	square finding is adapted from the squares.cpp sample included with opencv.

	Helper functions and samples are omitted from this header
*/

#pragma once
#ifndef SHAPEFINDER_H
#define SHAPEFINDER_H

#include "opencv2\opencv.hpp"
#include <vector>

// Find squares, including their centroids, longest sides, and shortest sides. 
void findSquares(const cv::Mat& image, std::vector<std::vector<cv::Point> >& squares, 
					std::vector<cv::Point> &centroids, std::vector<double> &longest_sides, 
					std::vector<double> &shortest_sides, int img_thresh, double side_diff_thresh, 
					bool downsample = true);

// Draw square
void drawSquares(cv::Mat& image, const std::vector<std::vector<cv::Point> >& squares);

// Calculate the longest side length of a single square (or rectangle)
double squareSideLength(std::vector<cv::Point> square, bool longest = true);

// Calculate the centroid of a single square (or any other shape)
cv::Point shapeCentroid(std::vector<cv::Point> square);

#endif