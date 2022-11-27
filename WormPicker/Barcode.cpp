/*
	Barcode.cpp
	Anthony Fouad
	Fang-Yen Lab
	Fall 2018

	LEGACY bar reading system (Anthony's HomeMade). 
	Not used in WW1.70+, replaced with QRCoder.

*/

// OpenCV includes
#include "opencv2\opencv.hpp"


// Standard includes
#include <vector>
#include <iostream>
#include <fstream>
#include <cmath>

// Anthony's includes
#include "Barcode.h"
#include "ImageFuncs.h"
#include "MatIndex.h"
#include "AnthonysCalculations.h"
#include "ShapeFinder.h"

// Namespaces
using namespace std;
using namespace cv;


// Constructor - does nothing
Barcode::Barcode(){

	// Set default values
	bar_int_ratio_thresh = 3.0;
	is_found = false;
	start_symb_height = 61;
	start_symb_height_tolerance = 0.02;
	bar_rise_thresh = 75;
	N_digits = 10;

	
}

// Constructor - scans right away
Barcode::Barcode(const cv::Mat& img) {

	// Set default values
	bar_int_ratio_thresh = 5.0;
	is_found = false;
	start_symb_height = 61;
	start_symb_height_tolerance = 0.02;
	bar_rise_thresh = 75;
	N_digits = 10;

	// Scan the image
	scanImage(img);
}

// Scan an image for the barcode
bool Barcode::scanImage(const Mat& img, bool downsample) {

	// Clear any previous barcode
	clearBarCode();

	// Create temporary storages for found shapes (cleared when passed to findSquares)
	vector<Point> square_centroids, rectangle_centroids;
	vector<double> square_longest_sides, square_shortest_sides, rectangle_longest_sides, rectangle_shortest_sides;

	// Find the start and end symbols - squares and rectangles
	///		* Try different thresholds until we (hopefully) find both the start and end
	///		* Just in case we do not, keep track of which thresholds yielded valid squares 
	///			that we can use for centering.
	vector<int> thresh_tried;
	vector<int> N_squares;
	int i = 0, j = 0;

	int thresh = 20;
	for (thresh = 50; thresh < 51; thresh += 25) {

		// Search for squares and rectangles

		findSquares(img, squares, square_centroids, square_longest_sides, square_shortest_sides, thresh, 1.0, downsample);	/// Squares
		findSquares(img, rectangles, rectangle_centroids, rectangle_longest_sides, rectangle_shortest_sides, thresh, 2.6, downsample); /// Rectangles
		//showAllShapes(img, rectangles, gg);

		// If we did not find a square and a rectangle, try again immediately
		thresh_tried.push_back(thresh);
		N_squares.push_back((int) squares.size());
 		if (squares.size() == 0 || rectangles.size() == 0) { continue; }

		// If we found too many squares or rectangles, try again immediately
		if (squares.size() > 5 || rectangles.size() > 5) { continue; }

		// Determine whether any of the found shapes are lined up appropriately to signal start and end (approx same row +/- 10 pixel)
		i = 0; j = 0;
		if (anySquareRectIsValid(i, j, square_centroids, rectangle_centroids, square_shortest_sides, rectangle_shortest_sides,img.rows)) {
			is_found = true;
			img_size = img.size();
			break;
		}
	}

	// If we did not find a square and a rectangle, we have no barcode. Return false
	if (!is_found)
		return false;

	// Store the barcode start and end symbols
	start = square_centroids[i];
	end = rectangle_centroids[j];
	start_symb = squares[i];
	start_symb_L = square_shortest_sides[i];
	end_symb_H = rectangle_shortest_sides[i];
	end_symb = rectangles[j];

	return is_found;
}


void Barcode::readBarCode(const cv::Mat& img) {

	// Exit if no barcode is found
	if (!is_found) {
		code_val = MatIndex(-1, -1);
		return;
	}

	// Determine read direction (+x or -x)
	/// If end is after start, then read_dir will be +1. Otherwise -1;
	read_dir = end.x - start.x;
	read_dir = read_dir / abs(read_dir);

	// calculate the background points and get intensities
	sampleSpacedPixels(0.5, bg_locs, bg_ints, img);

	// Calculate the the read points and get intensities
	sampleSpacedPixels(1.5, bar_locs, bar_ints, img);

	// Get the average background value
	double bg_mean = vectorMean(bg_ints);

	// Determine which bars are 1. Examine the start symbol to determine if this is darkfield or brightfield. 
	bar_vals.clear();
	for (int i = 0; i < bar_locs.size(); i++) {
		if (isDarkfield(img))
			bar_vals.push_back(bar_ints[i] / bg_mean > bar_int_ratio_thresh);
		else
			bar_vals.push_back(bar_ints[i] * bg_mean < bar_int_ratio_thresh );
	}

	// Translate binary to decimal (ROWS)
	code_val.row = binary2decimal(bar_vals, 0, (int) bar_vals.size() / 2 - 1);
	code_val.col = binary2decimal(bar_vals, (int) bar_vals.size() / 2, (int) bar_vals.size() - 1);

}

void Barcode::sampleSpacedPixels(double start_offset_bw_units, 
									vector<Point> &temp_locs, 
									vector<double> &temp_vals,
									const Mat& img)
{

	// Determine the start bound and end bound (start bound should be side of square closest to rect and vice versa)
	int start_x = 0, end_x = 0, dist = 9999;
	for (int i = 0; i < start_symb.size(); i++) {
		if (ptDist(start_symb[i], end) < dist) {
			start_x = start_symb[i].x;
			dist = (int) ptDist(start_symb[i], end);
		}
	}

	dist = 9999;
	for (int i = 0; i < end_symb.size(); i++){
		if (ptDist(end_symb[i], start) < dist) {
			end_x = end_symb[i].x;
			dist = (int) ptDist(end_symb[i], start);

		}
	}

	// Determine the thickness of the start symbol's outline
	/// width is 45 pixels compared to a desired read area of 2886 pixels, or 0.0156
	double shape_rel_thick = 0.0156;

	// Determine the width of the area to read from
	double read_width = (double) abs(start_x - end_x);
	read_width -= shape_rel_thick * 2 * read_width;
	double bar_width = read_width / (2 * N_digits + 1);		
	double current_offset = (start_offset_bw_units * bar_width + shape_rel_thick * read_width) * read_dir;	/// Current ofset from the end bound of the start symbol

	// First read point is 1.5 bar widths from start bound
	temp_locs.clear();
	temp_vals.clear();
	temp_locs.push_back(Point((int)(start_x + current_offset), start.y));
	temp_vals.push_back(img.at<uchar>(temp_locs[0]));

	// Read values and Fill in other read points
	for (int i = 1; i < N_digits; i++) {
		current_offset += 2 * bar_width * read_dir;
		temp_locs.push_back(Point((int)(start_x + current_offset), start.y));
		temp_vals.push_back(img.at<uchar>(temp_locs[i]));
	}
}

int Barcode::binary2decimal(vector<bool> data, int first_element, int last_element) {

	// Determine maximum exponent
	int max_exponent = last_element - first_element;

	// summ all 2^x exponents
	int sum = 0;
	int idx = first_element;
	int curr_exponent = max_exponent;

	for (int i = first_element; i <= last_element; i++) {
		if (data[i]) {
			sum += (int) pow((int)2, curr_exponent);
		}
		curr_exponent--;
	}

	// Return value
	return sum;

}

void Barcode::annotateImage(Mat& img) {

	// Exit if no barcode is found
	//if (!is_found)
	//	return;

	// Rescale coordinates to match the display image
	Point start_rs(0, 0), end_rs(0, 0);
	vector<Point> start_symb_rs, end_symb_rs, bar_locs_rs;

	if (is_found) {

		/// Scaling ratios
		double xratio = (double) img.cols / (double) img_size.width;
		double yratio = (double) img.rows / (double) img_size.height;

		/// Rescale start and end points
		start_rs = Point((int)(start.x * xratio), (int)(start.y * yratio));
		end_rs = Point((int)(end.x * xratio), (int)(end.y * yratio));

		/// Populate vector of rescaled start and end symbols, and bar locations
		for (int i = 0; i < start_symb.size(); i++)
			start_symb_rs.push_back(Point((int)(start_symb[i].x * xratio), (int)(start_symb[i].y * yratio)));

		for (int i = 0; i < end_symb.size(); i++)
			end_symb_rs.push_back(Point((int)(end_symb[i].x * xratio), (int)(end_symb[i].y * yratio)));

		for (int i = 0; i < bar_locs.size(); i++)
			bar_locs_rs.push_back(Point((int)(bar_locs[i].x * xratio), (int)(bar_locs[i].y * yratio)));
	}

	// Draw start symbol crosses
	drawCross(img, start_rs, colors::gg, 2, 12);
	drawCross(img, end_rs, colors::rr, 2, 12);

	// Draw symbol boundaries
	drawPlot(img, start_symb_rs, colors::gg, 1, true);
	drawPlot(img, end_symb_rs, colors::rr, 1, true);

	// Draw read locations
	for (int i = 0; i < bar_locs_rs.size(); i++) {
		circle(img, bar_locs_rs[i], 4, colors::gg , -1);

		if (bar_vals[i])
			putText(img, "1", bar_locs_rs[i], 1, 2, colors::gg);
		else
			putText(img, "0", bar_locs_rs[i], 1, 2, colors::gg);

	}

	// Draw overall read
	string translation;

	if (is_found)
		translation = "(" + to_string(code_val.row) + "," + to_string(code_val.col) + ")";
	else
		translation = "(-,-)";

	int cx = (int)(img.cols*0.005), cy = (int)(img.rows*0.995);
	rectangle(img, Rect(0, (int)(img.rows*0.95),(int)(img.cols*0.14),(int)(img.rows*0.07)), colors::kk, -1);
	putText(img, translation, Point(cx,cy), 1, 1.5, colors::pp,2);

}


void Barcode::clearBarCode() {
	start_symb.clear();
	end_symb.clear();
	bar_vals.clear();
	bar_locs.clear();
	bg_locs.clear();
	start = Point(0, 0);
	start_end = Point(0, 0);
	start_expected = Point(0, 0);
	end = Point(0, 0);
	end_begin = Point(0, 0);
	code_val = MatIndex(-1, -1);
	is_found = false;
}


bool Barcode::anySquareRectIsValid(int &i, int &j, 
									const vector<Point>& square_centroids,
									const vector<Point>& rectangle_centroids,
									vector<double> square_shortest_sides,
									vector<double> rectangle_shortest_sides,
									int H) 
{
	for (i = 0; i < square_centroids.size(); i++) {
		for (j = 0; j < rectangle_centroids.size(); j++) {
			if (abs(square_centroids[i].y - rectangle_centroids[j].y) < 0.1*H &&		/// Centroids required to be terminate together
				abs(rectangle_shortest_sides[i] - square_shortest_sides[i] <  0.01*H))	/// Sizes required to be similar
				return true;
		}
	}
	
	return false;
}

bool Barcode::isDarkfield(const cv::Mat& img) {

	// DISABLED - DARKFIELD MODE ONLY
	return true;

	// Determine the minimum and maximum values of the start symbol
	double min_val = 0, max_val = 0;
	Point min_idx(0,0), max_idx(0,0);

	Rect roi = boundingRect(start_symb);
	Mat img_test = img(roi);
	minMaxLoc(img, &min_val, &max_val, &min_idx, &max_idx);

	// Determine whether the value in the center of the symbol is closer to the max (brightfield) or min (darkfield)
	double center_val = img.at<uchar>(start);

	return abs(max_val - center_val) > abs(min_val - center_val);	
	/// Darkfield: 100 > 0 true
	/// Brightfield 0 > 100 false


}


MatIndex Barcode::getTranslation() const {
	return code_val;
}


// DEBUG FUNCTIONS

void Barcode::showAllShapes(Mat img, vector<vector<Point>> shapes, Scalar color) {

	// convert img to color if needed
	Mat img_color_temp;
	cvtColor(img, img_color_temp, COLOR_GRAY2RGB);

	// Show all squares
	for (int i = 0; i < shapes.size(); i++) {
		drawPlot(img_color_temp, shapes[i], color, 2, true);
	}

	// Display image
	namedWindow("DEBUG_SHOW_ALL_SHAPES", WINDOW_NORMAL);
	imshow("DEBUG_SHOW_ALL_SHAPES", img_color_temp);
	waitKey(0);
	exit(0);


}