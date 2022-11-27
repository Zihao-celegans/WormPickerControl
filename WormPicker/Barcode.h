/*
	Barcode.h
	Anthony Fouad
	Fang-Yen Lab
	Fall 2018

	Class for storing, reading, and translating WormWatcher / Picky style barcodes
*/
#pragma once
#ifndef BARCODE_H
#define BARCODE_H

// OpenCV includes
#include "opencv2\opencv.hpp"


// Standard includes
#include <vector>

// Anthony's includes
#include "MatIndex.h"
#include "AnthonysColors.h"

class Barcode {

public:
	// Constructor (does nothing or scans image)
	Barcode();
	Barcode(const cv::Mat& img);

	// Scan image to find the barcode (start and end symbols)
	bool scanImage(const cv::Mat& img, bool downsample = true);				/// Find barcode anywhere in the image

	// Read the barcode value
	void readBarCode(const cv::Mat& img);

	// Annotate image with barcode segmentation
	void annotateImage(cv::Mat& img);

	// Clear the bar code
	void clearBarCode();

	// Getter
	MatIndex getTranslation() const;


protected:

	// Protected functions for local use

	/// DEBUG FUNCTION - Show all shapes  - add this func to show all squares or rectangles on a color image
	void showAllShapes(cv::Mat img, std::vector<std::vector<cv::Point>> shapes, cv::Scalar color = colors::gg);

	/// Create evenly spaced sample pixles for reading the barcode
	void sampleSpacedPixels(double start_offset_bw_units, std::vector<cv::Point> &temp_locs, std::vector<double> &temp_vals, const cv::Mat& img);

	/// Convert a portion of a vector of bools into a decimal into a decimal number. To use the whole vector supply 0 and data.size() as the second and third inputs
	int binary2decimal(std::vector<bool> data, int first_element, int last_element);

	/// Determine whether any of the squares or rectangles whose centroids are found are a valid start/end pair (same row)
	bool Barcode::anySquareRectIsValid(int &i, int &j,
		const std::vector<cv::Point>& square_centroids,
		const std::vector<cv::Point>& rectangle_centroids,
		std::vector<double> square_shortest_sides,
		std::vector<double> rectangle_shortest_sides, 
		int H);

	/// Determine whether the given image is darkfield (sqyare center is dark) or bright field (square center is light)
	bool isDarkfield(const cv::Mat& img);

	// Shape coordinates and values
	std::vector<bool> bar_vals;		/// 10 digit binary code
	std::vector<double> bar_ints;	/// Intensities at each bar
	std::vector<double> bg_ints;	/// Intensities at each bar

	std::vector<cv::Point> bar_locs; /// Locations sampled for 1s or 0s
	std::vector<cv::Point> bg_locs; /// Locations known to be background, for a dark reference

	MatIndex code_val;				/// Row (first 5 digits) and col (second 5 digits) translated to decimal
	cv::Point start, start_expected;/// Actual and expected coordinates of the start symbol (x,y)
	cv::Point start_end;			/// End boudnary of the start symbol
	cv::Point end;
	cv::Point end_begin;			/// Left most edge of the end symbol
	std::vector<cv::Point> start_symb, end_symb; /// 4 point rectangles storing the start and end symbols
	cv::Size img_size;				/// Image size used for reading, in case we have to resize the coords later
	std::vector<std::vector<cv::Point> > squares, rectangles; /// List of all found squares and rectangles.
	double start_symb_L, end_symb_H;	/// Size of the start and end symbols

	int read_dir;

	// Parameters
	bool is_found;					/// Whether the object is currently storing a valid barcode. 
	int start_symb_height;			/// Height of the start and end symbol. Also width of the start symbol (a square)
	double start_symb_height_tolerance;	/// Fraction different allowed when finding start symbol.
	int bar_rise_thresh;					/// Value of pixel derivative to form a valid bar. 
	int N_digits;					/// 10, half for row, half for col, must be even.
	double bar_int_ratio_thresh;			/// 

	// Shared temporary data
	cv::Point temp;					/// Shared point used for row scanning
	int x0, x1, y0, y1;				/// temporary boundaries used for row scanning

};

#endif