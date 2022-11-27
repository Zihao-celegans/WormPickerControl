/*
	ShapeFinder.cpp
	Anthony Fouad
	Fang-Yen Lab
	Fall 2018

	Find specific types of shapes within an image.
	square finding is adapted from the squares.cpp sample included with opencv. 
*/

#include "opencv2\opencv.hpp"

#include <iostream>
#include <math.h>
#include <string.h>
#include <vector>

#include "AnthonysTimer.h"
#include "AnthonysCalculations.h"
#include "ShapeFinder.h"
#include "ImageFuncs.h"

using namespace cv;
using namespace std;

// helper function:
// finds a cosine of angle between vectors
// from pt0->pt1 and from pt0->pt2
double angle(Point pt1, Point pt2, Point pt0)
{
	double dx1 = pt1.x - pt0.x;
	double dy1 = pt1.y - pt0.y;
	double dx2 = pt2.x - pt0.x;
	double dy2 = pt2.y - pt0.y;
	return (dx1*dx2 + dy1 * dy2) / sqrt((dx1*dx1 + dy1 * dy1)*(dx2*dx2 + dy2 * dy2) + 1e-10);
}

// returns sequence of squares (or rectangles, if specified) detected on the image.
//		* the sequence is stored in the specified memory storage
//		* img_thresh is usually about 100.
//		* side_diff_thresh should be 1 for squares, and 2 for rectangles with 2L = H
//		Adapted from the sample code in squares.cpp included with OpenCV
void findSquares(const Mat& image, vector<vector<Point> >& squares, 
				vector<Point> &centroids, vector<double> &longest_sides, vector<double> &shortest_sides,
				int img_thresh, double side_diff_thresh, bool downsample)
{

	// Setup variables
	squares.clear();
	centroids.clear();
	longest_sides.clear();
	shortest_sides.clear();
	Mat pyr, timg, gray0(image.size(), CV_8U), gray;

	// down-scale and upscale the image to filter out the noise
	pyrDown(image, pyr, Size(image.cols / 2, image.rows / 2));
	pyrUp(pyr, timg, image.size());

	// Downsample the image to improve speed
	if (downsample) {
		resize(timg, timg, Size(), 0.5, 0.5);
		resize(gray0, gray0, Size(), 0.5, 0.5);
	}

	vector<vector<Point> > contours;

	// find squares only in the gray plane, regardless of whether it is color or grayscale
	int c = 0;
	int ch[] = { c, 0 };
	mixChannels(&timg, 1, &gray0, 1, ch, 1);


	// Subtract low frequency and the filter noise with a low threshold
	GaussianBlur(gray0, gray, Size(33, 33), 55, 55);
	gray = gray0 - gray > 1;

	// Morphological terminate (dilation then erosion)
	dilate(gray, gray, getStructuringElement(MORPH_ELLIPSE, Size(9,9)));
	erode(gray, gray, getStructuringElement(MORPH_ELLIPSE, Size(9,9)));

	// find contours and store them all as a list
	findContours(gray, contours, RETR_LIST, CHAIN_APPROX_SIMPLE);
	vector<Point> approx;

	/*Mat temp;
	cvtColor(gray, temp, CV_GRAY2RGB);
	drawContours(temp, contours, -1, gg, 1);
	showDebugImage(temp);*/

	// test each contour
	for (size_t i = 0; i < contours.size(); i++)
	{
		// approximate contour with accuracy proportional
		// to the contour perimeter
		approxPolyDP(Mat(contours[i]), approx, arcLength(Mat(contours[i]), true)*0.02, true);

		// square contours should have:
		//		* 4 vertices after approximation
		//		* relatively large area (to filter out noisy contours)
		//		* be convex, and have angles <= 90 degrees.
		// Note: absolute value of an area is used because
		// area may be positive or negative - in accordance with the
		// contour orientation
		if (approx.size() >= 4 &&
			fabs(contourArea(Mat(approx))) > 900 && 
			isContourConvex(Mat(approx)))
		{
			// find the maximum cosine of the angle between joint edges
			double maxCosine = 0;
			for (int j = 2; j < 5; j++)
			{
				double cosine = fabs(angle(approx[j % 4], approx[j - 2], approx[j - 1]));
				maxCosine = MAX(maxCosine, cosine);
			}

			// find the maximum difference between side lengths
			double min_side = 9999, max_side = 0;
			bool max_side_horizontal = false;
			for (int j = 2; j < 5; j++) {
				Point pt0 = approx[j % 4];
				Point pt1 = approx[(j % 4 + 1) % 4];
				double side_length = ptDist(pt0,pt1);
				if (side_length < min_side) { min_side = side_length; }
				if (side_length > max_side) { 
					max_side = side_length; 
					max_side_horizontal = abs(pt0.x - pt1.x) > abs(pt0.y - pt1.y); // If the x-dist between points is greater, it's horizontal.
				}
			}

			// Is this a horizontal rectangle? TRUE if it's a horiz rectangle OR a square.
			bool is_horizontal_rectangle_or_square =  (side_diff_thresh > 1.5 && max_side_horizontal) || side_diff_thresh <= 1.5;


			// Store this as a square If: 
			//	* Cosines of all angles are small (all angles are ~90 degree) 
			//	* Maximum length difference between sides is within 10% of the specified
			//	* SPECIAL CASE FOR RECTANGLES: REQUIRED TO BE HORIZONTALLY ORIENTED


			if (maxCosine < 0.3 &&
				max_side / min_side < side_diff_thresh + 0.1 &&
				max_side / min_side > side_diff_thresh - 0.1 && 
				is_horizontal_rectangle_or_square)
			{
				/// Restore "approx" to the correct scaling
				if (downsample) {
					for (int j = 0; j < approx.size(); j++)
						approx[j] = Point(approx[j].x * 2, approx[j].y * 2);
				}

				squares.push_back(approx);							/// Square coordinates
				centroids.push_back(shapeCentroid(approx));			/// Centroid
				longest_sides.push_back(squareSideLength(approx, true));	/// Longest side
				shortest_sides.push_back(squareSideLength(approx, false));/// Shortest side
			}
		}
	}
}


// Draws all the squares or rectangles in the image
void drawSquares(Mat& image, const vector<vector<Point> >& squares)
{
	for (size_t i = 0; i < squares.size(); i++)
	{
		const Point* p = &squares[i][0];
		int n = (int)squares[i].size();
		polylines(image, &p, &n, 1, true, Scalar(0, 255, 0), 1, LINE_AA);
	}

	imshow("Figure 1", image);
}

//Calculate the longest (or shortest) side length of a square or rectangle
double squareSideLength(vector<Point> square, bool longest) {

	// find the max and min side lengths
	double min_side = 9999, max_side = 0;
	for (int j = 2; j < 5; j++) {
		double side_length = ptDist(square[j % 4], square[(j % 4 + 1) % 4]);
		if (side_length < min_side) { min_side = side_length; }
		if (side_length > max_side) { max_side = side_length; }
	}

	// Return the requested value
	if (longest) { return max_side; }
	else { return min_side; }
}

// Calculate the centroid of the square
Point shapeCentroid(vector<Point> shape) {

	double xbar = 0, ybar = 0;

	for (int i = 0; i < shape.size(); i++) {
		xbar += shape[i].x;
		ybar += shape[i].y;
	}

	return Point(xbar / shape.size(), ybar / shape.size());
}


// Example usage (omitted from header file)
//int SquareFindingExample(int argc, char** argv)
//{
//	static const char* names[] = { "C:\\Dropbox\\Anthony Team (shared)\\Robot Materials\\WormWatcherTray\\sample_barcode_2.png", 0 };
//
//	if (argc > 1)
//	{
//		names[0] = argv[1];
//		names[1] = "0";
//	}
//
//	namedWindow("Figure 1", 1);
//	vector<vector<Point> > squares;
//
//	for (int i = 0; names[i] != 0; i++)
//	{
//		Mat image = imread(names[i], 1);
//		if (image.empty())
//		{
//			cout << "Couldn't load " << names[i] << endl;
//			continue;
//		}
//		AnthonysTimer Ta;
//		findSquares(image, squares, 100, 2);
//		cout << "It took " << Ta.getElapsedTime() << " seconds" << endl;
//		drawSquares(image, squares);
//
//		char c = (char)waitKey();
//		if (c == 27)
//			break;
//	}
//
//	return 0;
//}
//
//
