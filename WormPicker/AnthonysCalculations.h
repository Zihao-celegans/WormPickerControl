/*
	AnthonysCalculations.h
	Anthony Fouad
	Fang-Yen Lab
	March 2018

	Functions for basic mathematical and file handling operations.
*/

#pragma once
#ifndef ANTHONYSCALCULATIONS_H
#define ANTHONYSCALCULATIONS_H

// Standard includes
#include <vector>
#include <algorithm>
#include <iterator>
#include <fstream>

// OpenCV includes
#include "opencv2\opencv.hpp"

/*
	Find the index of the first occurrence of element in vector.
	If not found, return T=-1
	https://www.geeksforgeeks.org/how-to-find-index-of-a-given-element-in-a-vector-in-cpp/
*/

template<class T>
int vectorFind(const std::vector<T> &v, T element){
	auto it = find(v.begin(), v.end(), element);

	// If element was found
	if (it != v.end())
	{
		// calculating the index of K
		int index = it - v.begin();
		return index;
	}
	else {
		// If the element is not present in the vector
		return -1;
	}
	
}


/*
	Determine whether the element is in the set.
	Return true if in the set, otherwise, false
*/

template<class T>
bool setFind(std::set<T> &s, T element) {
	return (s.find(element) != s.end());
}


template<class T>
T vectorMedian(std::vector<T> &v);

/*
	Calculates median of elements in vector of doubles, ints, etc.
		- Note that the template has to be redeclared prior to the function definition below.
		- Note that the function using the template needs to be in the header file itself, not in a separate CPP file. 
		- nth_element partially sorts the list so that the element at address v.begin+n is the
			element that WOULD be there if the vector were actually sorted.
*/

template<class T>
T vectorMedian(std::vector<T> &v)
{
	if (v.size()>0) {
		size_t n = v.size() / 2;
		nth_element(v.begin(), v.begin() + n, v.end());
		return v[n];
	}
	else {
		return 999999;
	}
}

// Calculate the mean of elements` in a vector
template<class T>
double vectorMean(std::vector<T> &v)
{
	if (v.size()>0) {
		double mean_val = 0;
		for (int i = 0; i < v.size(); i++) {
			mean_val += v[i];
		}
		return mean_val/v.size();
	}
	else {
		return -999999;
	}
}

// Calculate the standard deviation of elements in vector
template <class B>
float vectorStd(std::vector<B> vec) {
	float variance = 0.0;
	float SD;
	float mean = vectorMean(vec);

	for (int i = 0; i < vec.size(); i++) {
		variance += pow(vec[i] - mean, 2);
	}
	variance = variance / vec.size();
	SD = sqrt(variance);
	return SD;

}


// Calculate the sum of elements in a vector
template<class T>
T vectorSum(std::vector<T>& v) {

	T sum = 0;
	for (int i = 0; i < v.size(); i++) {
		sum = sum + v[i];
	}

	return sum;
}

// Calculate the maximum of elements in a vector, and also the idx of the maximum element
template<class T>
T vectorMax(std::vector<T>& v, int* ptr_max_idx = nullptr) {

	*ptr_max_idx = -1;
	T max_val = -999999;
	for (int i = 0; i < v.size(); i++) {
		if (v[i] > max_val) {
			max_val = v[i];
			*ptr_max_idx = i;
		}
	}

	return max_val;
}

// Calculate the minimum of elements in a vector, and also the idx of the minimum element
template<class T>
T vectorMin(std::vector<T>& v, int* ptr_min_idx = nullptr) {

	*ptr_min_idx = -1;
	T min_val = 999999;
	for (int i = 0; i < v.size(); i++) {
		if (v[i] < min_val) {
			min_val = v[i];
			*ptr_min_idx = i;
		}
	}

	return min_val;
}

// Normalize the elements of a vector (in place). 
// Be careful if using ints for a range that requires decimals like [0,1], won't work
// Be careful with negative elements
// Ideal case is vector of doubles in [0, 255]

template<class T>
void vectorNorm(std::vector<T>& v, double max_limit) {

	// Subtract the minimum value
	T min_val = vectorMin(v);
	for (int i = 0; i < v.size(); i++) 
		v[i] -= min_val;

	// Scale so that highest value is maximum
	T max_val = vectorMax(v);
	for (int i = 0; i < v.size(); i++)
		v[i] *= (double)max_limit / (double)max_val;
	

	return;
}

// Downsample a vector to a shorter length, averaging the excess points in each bin
template<class P>
std::vector<P> vectorDownsample(std::vector<P> v, int L) {

	// Return original vector if downsampling is not indicated
	if (L >= v.size() || v.size() == 0)
		return v;

	// Figure out spacing in order to achieve length L
	int spacing = (int) round(v.size() / L);
	std::vector<P> new_vect;

	for (int i = 0; i < L; i++) {

		// Start and end indices for this bin
		int i0 = i * spacing;
		int i1 = (int) std::min((double) i0 + spacing, (double) v.size());

		// Get average
		std::vector<P> sub_v(v.begin() + i0, v.begin() + i1);
		P avg_val = vectorMean(sub_v);
		new_vect.push_back(avg_val);
	}

	return new_vect;
}


/* 
	Flatten a vector of vectors into a single vector by taking the average

	- Specify the use_median (0 = mean, default, 1= median)
	- All vectors must be the same length

*/

template<class P>
std::vector<P> vectorOfVectorsFlatten(std::vector<std::vector<P>> vv, bool use_median = 0) {

	// Validate input vector
	std::vector<P> vflat;
	int L = vv[0].size();
	int N = vv.size();

	if (N < 1)
		throw std::runtime_error("vector of vectors to be flattened is empty");

	// Take the average of the j'th element in each subvector
	
	for (int j = 0; j < L; j++) {

		std::vector<P> jth_elems;

		for (int i = 0; i < N; i++) {

			// Validate sub-vector's matching length
			if (i > 0 && vv[i].size() != L)
				throw std::runtime_error("Subvector lengths do not match, cannot flatten");

			// Store the jth element of this sub vector
			jth_elems.push_back(vv[i][j]);
		}

		// Get the mean or median of all jth elems in all the vectors
		if (!use_median)
			vflat.push_back(vectorMean(jth_elems));
		else
			vflat.push_back(vectorMedian(jth_elems));
	}

	// Ensure vflat has correct length
	if (vflat.size() != L)
		throw std::runtime_error("Result vflat length does not match input lengths");

	// Return vflat
	return vflat;

}

// Calculate the distance between two CV points, possibly different classes of points
template<class P, class Q>

double ptDist(P p1, Q p2) {
	return sqrt(pow(p1.x - p2.x, 2) + pow(p1.y - p2.y, 2));
}

// Calculate the distance between two CV points, weighting x and y differently
template<class P>

double ptDist(P p1, P p2, double wx, double wy) {
	return sqrt(wx*pow(p1.x - p2.x, 2) + wy*pow(p1.y - p2.y, 2));
}

// find the closest point to a reference CV point, 
template<class P>

int closestPoint(std::vector<P> pts_list, P ref_pt) {

	int idx = -1;
	double min_dist = 99999;
	for (int i = 0; i < pts_list.size(); i++) {
		if (ptDist(ref_pt, pts_list[i]) < min_dist) {
			min_dist = ptDist(ref_pt, pts_list[i]);
			idx = i;
		}
	}
	return idx;
}

// OVERLOAD: find the closest point to a reference CV point using DEQUE instead of VECTOR
template<class P>

int closestPoint(std::deque<P> pts_list, P ref_pt) {

	int idx = -1;
	double min_dist = 99999;
	for (int i = 0; i < pts_list.size(); i++) {
		if (ptDist(ref_pt, pts_list[i]) < min_dist) {
			min_dist = ptDist(ref_pt, pts_list[i]);
			idx = i;
		}
	}
	return idx;
}

// OVERLOAD: find the closest point to a reference CV point, weight x and y differently
template<class P>

int closestPoint(std::vector<P> pts_list, P ref_pt, double wx, double wy) {

	int idx = -1;
	double min_dist = 9999999999;
	for (int i = 0; i < pts_list.size(); i++) {
		if (ptDist(ref_pt, pts_list[i], wx, wy)  < min_dist)  {
			min_dist = ptDist(ref_pt, pts_list[i], wx, wy);
			idx = i;
		}
	}
	return idx;
}

// Overload: find the N closest points to a reference point
template<class P>
std::vector<int> closestPoint(std::vector<P> pts_list, P ref_pt, int N) {

	// N cannot be larger than the number of available points
	N = min(N, pts_list.size());

	// Get N closest points
	std::vector<int> closest;
	for (int i = 0; i < N; i++) {
		int idx = closestPoint(pts_list, ref_pt);
		closest.push_back(idx);						/// get the point
		pts_list[idx] = Point(999999, 999999);		/// Remove from consideration
	}

	// Return results
	return closest;
}

// Find the closest x or y value to a given value
template<class T>
int closestVal(std::vector<T> vals, T ref_val) {
	int idx = -1;
	double min_dist = 99999;
	for (int i = 0; i < vals.size(); i++) {
		if (ptDist(ref_val, vals[i]) < min_dist) {
			min_dist = abs(ref_val - vals[i]);
			idx = i;
		}
	}
	return idx;
}

// Compute correlation coefficient between two vectors
template<class T>
double corrcoef(const std::vector<T>& X, const std::vector<T>& Y) {

	// Make sure two vectors have same length
	if (X.size()!=Y.size()) {
		cout << "Fail to calculate correlation coefficient: Unequal dimension!" << endl;
		return -99999;
	}
	else {

		int n = X.size();
		double sum_X = 0, sum_Y = 0, sum_XY = 0;
		double sumSq_X = 0, sumSq_Y = 0;

		for (int i = 0; i < n; i++) {
			// Sum of elements of vector X
			sum_X = sum_X + X[i];

			// Sum of elements of vector Y
			sum_Y = sum_Y + Y[i];

			// Sum of X[i] * Y[i]
			sum_XY = sum_XY + X[i] * Y[i];

			// Sum of square of vector elements
			sumSq_X = sumSq_X + X[i] * X[i];
			sumSq_Y = sumSq_Y + Y[i] * Y[i];
		}

		// Compute correlation coefficient
		double corr = (double)(n * sum_XY - sum_X * sum_Y)
			/ sqrt((n * sumSq_X - sum_X * sum_X) * (n * sumSq_Y - sum_Y * sum_Y));

		return corr;
	}
}

// Calculate the center of mass of a group of points with different weight
template<class T>
void CenterofMass(double& ctr_mass_x, double& ctr_mass_y, std::vector<T>& pts_x, std::vector<T>& pts_y, std::vector<double>& weights) {
	if (pts_x.size() == pts_y.size() && pts_x.size() == weights.size() && pts_x.size() > 0) {

		//for (int j = 0; j < weights.size(); j++) {
		//	if (weights[j] < 0) {
		//		weights[j] = 0;
		//	}
		//}

		double weighted_sum_x = 0;
		double weighted_sum_y = 0;
		for (int i = 0; i < pts_x.size(); i++) {
			weighted_sum_x += weights[i] * pts_x[i];
			weighted_sum_y += weights[i] * pts_y[i];
		}

		double sum_weights = vectorSum(weights);
		if (vectorSum(weights) != 0) {
			
			ctr_mass_x = weighted_sum_x / sum_weights;
			ctr_mass_y = weighted_sum_y / sum_weights;
		}
		else {
			// cout << "Fail to calculate center of mass: sum of mass is 0" << endl;
			ctr_mass_x = -1;
			ctr_mass_y = -1;
		}
	
	}
	else {
		cout << "Fail to calculate center of mass: empty vector or unequal vector size" << endl;
		ctr_mass_x = -1;
		ctr_mass_y = -1;
	}

}

// Overloaded version of centerofmass: calculate the center of mass of a single vector. The position will be the vector index, weights will be the value.
template<class T>
int CenterofMass(std::vector<T>& weights) {
	
	if (weights.size() > 0) {

		double weighted_sum = 0;
		for (int i = 0; i < weights.size(); i++) {
			weighted_sum += weights[i] * i;
		}

		double sum_weights = vectorSum(weights);

		if (vectorSum(weights) != 0) {
			double ctr_mass_idx = weighted_sum / sum_weights;
			return (int)ctr_mass_idx;
		}
		else {
			// cout << "Fail to calculate center of mass: sum of mass is 0" << endl;
			return -1;
		}
	
	
	}
	else { 
		cout << "Fail to calculate center of mass: empty vector!" << endl;
		return -1; 
	}

}




// Calculate the MEDIAN of a vector of points. Use center of mass or getCentroid()
//     for traditional mean
template<class P>
P contourMedian(std::vector<P> pts) {

	// Strip points into x and y
	std::vector<double>x, y;
	for (int i = 0; i < pts.size(); i++) {
		x.push_back(pts[i].x);
		y.push_back(pts[i].y);
	}

	// Median of x and y
	double med_x = vectorMedian(x);
	double med_y = vectorMedian(y);
	return P(med_x, med_y);
}

// Swap the axis of vector of vector e.g. {{1,2,3}, {4,5,6}} -> {{1,4}, {2,5}, {3,6}}
template<class T>
void SwapAxis(const std::vector<std::vector<T>>& srcV, std::vector<std::vector<T>>& dstV) {
	// Check if all the secondary vectors have the same dimension
	bool is_same_size = true;
	for (int i = 0; i < srcV.size(); i++) {
		if (srcV[0].size() != srcV[i].size()) { is_same_size = false; break; }
	}

	if (is_same_size && !srcV.empty()) {
		for (int i = 0; i < srcV[0].size(); i++ ) {

			vector<T> v;
			for (int j = 0; j < srcV.size(); j++) {
				v.push_back(srcV[j][i]);
			}

			dstV.push_back(v);
		}
	}
}

// Find local minima in a series of doubles, returning their integer location. First and last cannot be included
std::vector<cv::Scalar> findMinima(std::vector<double> v, double min_prom = 0);

// Convert all numbers within a string to doubles. Numbers are delimited by non-numeric characters other than ".", the decimal point. 
std::vector<double> string2double(std::string str);

// Convert one number, whose digits are encoded in digits, to a double. -88 is negative, -99 is decimal.
double vector2double(std::vector<int> digits);

// Calculate the angle between three cv::Points
int ptAngle(cv::Point p1, cv::Point p2, cv::Point p3);

//calculate the centroid of a contour
cv::Point getCentroid(std::vector<cv::Point> contour);

// Isolate longest chain of cv::Point's within the vector of points
void isolate_longest_chain(std::vector<cv::Point> &point_vec, int col_interval, int min_pick_points);

// Isolate longest chain of cv::Point's separated by Y-jumps
void isolate_longest_chain_by_y_jump(std::vector<cv::Point> &point_vec, int max_x_jump, int max_y_jump, int min_pick_points);

// Check if a file path is valid.
bool validate_path(std::string& userpath);
bool makePath(std::string userpath);

// Load file names of all videos from a file path
std::vector<std::string> getFilesInPath(std::string path, std::string fileext = ".avi");

// Get current timestamp
std::string getCurrentTimeString(std::vector<double> &time_vals, bool ms = false, bool round_to_s = false);
std::string getCurrentTimeString(bool ms = false, bool round_to_s = false);
std::string getCurrentDateString();
double getCurrentHour();
double getCurrentMinute();
double getCurrentSecond();

// Execute external EXE, such as a compiled matlab program
std::string executeExternal(const char* cmd);

// Print an error message to both cmd and a debugging log, with current timestamp
void printTimedError(std::string msg, std::ofstream& debug_log, std::string fname_email_program = "", std::string fname_email_alert = "");

// Prints an error message as above when the deadline time runs out or when *cancel is set to true. Returns immediately.
void printTimedErrorAtDeadline(std::string msg, std::ofstream& debug_log, std::string fname_email_program = "", std::string fname_email_alert = "",
	int deadline_time_s = 10, bool *cancel = nullptr);

// Check if a vector of strings contains string s
bool vsContains(std::vector<std::string> vs, std::string s);

// Calculate a duration (e.g. remaining) in an appropriate unit (e.g. hours, minutes, days)
std::string durationString(int seconds, std::string suffix = " remain");

// Vector extrapolation
void VecExtrapolate(std::vector<int>& in_vec, std::vector<int>& out_vec, int up_lim, int low_lim);

// Compute y / x value for a point on a line, given its x / y value
int getlineYfromX(cv::Vec4f line_vec, int X);
int getlineXfromY(cv::Vec4f line_vec, int Y);

// Compute differences between the adjcent elements in the vector
template <class T>
void AdjDifference(std::vector<T>& vec_in, std::vector<T>& vec_out);


// Points shift by a certain offset in X and Y direction: dX and dY
void PointsOffset(std::vector<cv::Point>& pts_vec_in, std::vector<cv::Point>& pts_vec_out, int dX, int dY);
#endif