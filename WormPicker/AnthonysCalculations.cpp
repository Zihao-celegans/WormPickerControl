#include "AnthonysCalculations.h"
#include <sys/types.h>
#include <sys/stat.h>

// Standard includes
#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <iterator>


// Time includes
#include <ctime>
#include <time.h>

// Dirent includes
#include "dirent.h"

// Boost includes
#include "boost\asio.hpp"
#include "boost\filesystem.hpp"

// Anthony's includes
#include "AnthonysTimer.h"
#include "ImageFuncs.h"
#include "ThreadFuncs.h"
#include "ControlledExit.h"

// Namespaces
using namespace std;
using namespace cv;


vector<Scalar> findMinima(vector<double> v, double min_prom) {

	// Find all minima: Decreasing then increasing, and maxima
	vector<int> idx_min;
	vector<double> vals_min;
	vector<Scalar> out;

	for (int i = 1; i < v.size() - 1; i++) {
		if (v[i] - v[i - 1] < 0 && v[i + 1] - v[i] > 0) {
			idx_min.push_back(i);
			vals_min.push_back(v[i]);
		}
	}

	// Filter peaks according to prominence
	// Adapted from: https://www.mathworks.com/help/signal/ug/prominence.html
	for (int i = 0; i < idx_min.size(); i++) {

		// STEP 1a: find the first peak LOWER than this minima on left 
		int left_ref = 0;
		for (int j = i-1; j >=0 ; j--) {
			if (vals_min[j] < vals_min[i]) {
				left_ref = idx_min[j];
				break;
			}
		}

		// STEP 1b: find first peak LOWER than this minima on right
		int right_ref = v.size() - 1;
		for (int j = i+1; j < idx_min.size(); j++) {
			if (vals_min[j] < vals_min[i]) {
				right_ref = idx_min[j];
				break;
			}
		}

		// STEP 2: find the maximum of the signal in this range. This is the reference level
		double ref_max = 0;
		for (int j = left_ref; j <= right_ref; j++) {
			if (v[j] > ref_max) {
				ref_max = v[j];
			}
		}

		// STEP 3: Prominence = abs(peak - reference)
		double prominence = 1 - (vals_min[i] / ref_max);

		// STEP 4: Keep this peak minima if promience threshold passed
		if (prominence > min_prom) {
			out.push_back(Scalar(idx_min[i],vals_min[i],prominence));
		}
	}

	return out;
}


vector<double> string2double(string str) {

	int decimal_pos = -1;
	vector<int> digits;
	vector<double> all_nums;

	for (int i = 0; i < str.size(); i++) {

		// Decimal point
		if (!str.substr(i, 1).compare(".") && decimal_pos == -1) {
			digits.push_back(-99);
			decimal_pos = i;
		}

		// Negative sign (must occur on empty digits)
		else if (!str.substr(i, 1).compare("-") && digits.empty()) {
			digits.push_back(-88);
		}
			

		// Numbers
		else if (!str.substr(i, 1).compare("0"))
			digits.push_back(0);

		else if (!str.substr(i, 1).compare("1"))
			digits.push_back(1);

		else if (!str.substr(i, 1).compare("2"))
			digits.push_back(2);

		else if (!str.substr(i, 1).compare("3"))
			digits.push_back(3);

		else if (!str.substr(i, 1).compare("4"))
			digits.push_back(4);

		else if (!str.substr(i, 1).compare("5"))
			digits.push_back(5);

		else if (!str.substr(i, 1).compare("6"))
			digits.push_back(6);

		else if (!str.substr(i, 1).compare("7"))
			digits.push_back(7);

		else if (!str.substr(i, 1).compare("8"))
			digits.push_back(8);

		else if (!str.substr(i, 1).compare("9"))
			digits.push_back(9);


		// Non-numeric, unexpected decimal or negative: delimit the number unless empty.
		/// NOTE: If dashes are used for dlimiting, dashes BETWEEN valid numbers will not be
		/// considered negative, but a dash BEFORE the first number will be. Avoid using dashes
		else if (!digits.empty()){
			all_nums.push_back(vector2double(digits));
			digits.clear();
			decimal_pos = -1;
		}

		// Non-numeric, unexpected decimal points and nothing in 
		else {
			digits.clear();
		}
	}

	// In case string ended with no delimiters:
	if(!digits.empty())
		all_nums.push_back(vector2double(digits));

	return all_nums;
}

double vector2double(vector<int> digits) {

	// Read from back. -99 is decimal, -88 is negative. Read until the decimal or negative
	double total = 0;
	int current_power = 0, multiplier = 1;

	for (int i = (int) digits.size() - 1; i >= 0; i--) {

		// Reached decimal from the right: convert total to decimal
		if (digits[i] == -99) {
			total /= pow(10, current_power);
			current_power = 0;

		}

		// Negative sign
		else if (digits[i] == -88) {
			multiplier = -1;
		}
		
		// Digit
		else if (digits[i] >= 0 && digits[i] < 10){
			total += pow(10, current_power++)*digits[i];
		}

		// Invalid digit or misplaced minus sign
		else {
			return -99999;
		}
	}

	total *= multiplier;
	return total;

}


int ptAngle(cv::Point a, cv::Point b, cv::Point c)
{
	cv::Point ab = { b.x - a.x, b.y - a.y };
	cv::Point cb = { b.x - c.x, b.y - c.y };

	float dot = (ab.x * cb.x + ab.y * cb.y);   // dot product
	float cross = (ab.x * cb.y - ab.y * cb.x); // cross product
	float alpha = atan2(cross, dot);

	return (int)floor(alpha * 180. / 3.14159 );
}

cv::Point getCentroid(std::vector<cv::Point> contour) {
	Moments mu = moments(contour);
	Point2f f_centroid = Point2f(mu.m10 / mu.m00, mu.m01 / mu.m00);
	return Point((int)f_centroid.x, (int)f_centroid.y);
}


void isolate_longest_chain(vector<Point> &point_vec, int col_interval, int min_pick_points) {

	if (point_vec.size() >= min_pick_points) {

		/// Seed with first point as beginning of first segment
		vector <int> segment_start_idx, segment_length;
		segment_start_idx.push_back(0);
		segment_length.push_back(1);

		/// Cycle through all other points to measure segments
		int discontinuity_jump = col_interval * 2;

		for (int i = 1; i < point_vec.size(); i++) {

			/// If it's distant from previous point (more than 3 points distant), start a new object
			if (point_vec[i].x - point_vec[i - 1].x > discontinuity_jump) {
				segment_start_idx.push_back(i);
				segment_length.push_back(1);
			}

			/// Else add to length of current object
			else {
				++segment_length[segment_length.size() - 1];
			}
		}

		/// Determine which object was the longest
		int i_longest = 0, L_longest = 1;
		for (int i = 0; i < segment_length.size(); i++) {
			if (segment_length[i] > L_longest) {
				i_longest = segment_start_idx[i];
				L_longest = segment_length[i];
			}
		}

		/// Isolate this object into point_vec
		vector<Point> point_vec_temp = point_vec;
		point_vec.clear();
		for (int i = i_longest; i <= i_longest + L_longest - 1; ++i) {
			point_vec.push_back(point_vec_temp[i]);
		}
	}

}


// Isolate longest chain of cv::Point's separated by Y-jumps

void isolate_longest_chain_by_y_jump(vector<Point> &point_vec, int max_x_jump, int max_y_jump, int min_pick_points) {

	if (point_vec.size() >= min_pick_points) {

		/// Seed with first point as beginning of first segment
		vector <int> segment_start_idx, segment_length;
		segment_start_idx.push_back(0);
		segment_length.push_back(1);

		/// Cycle through all other points to measure segments

		for (int i = 1; i < point_vec.size(); i++) {

			/// If it's distant from previous point (more than 3 points distant), start a new object
			bool invalid_y_jump = abs((double)point_vec[i].y - (double)point_vec[i - 1].y) > max_y_jump;
			bool invalid_x_jump = abs((double)point_vec[i].x - (double)point_vec[i - 1].x) > max_x_jump; 
			if (invalid_y_jump || invalid_x_jump) {
				segment_start_idx.push_back(i);
				segment_length.push_back(1);
			}

			/// Else add to length of current object
			else {
				++segment_length[segment_length.size() - 1];
			}
		}

		/// Determine which object was the longest
		int i_longest = 0, L_longest = 1;
		for (int i = 0; i < segment_length.size(); i++) {
			if (segment_length[i] > L_longest) {
				i_longest = segment_start_idx[i];
				L_longest = segment_length[i];
			}
		}

		/// Isolate this object into point_vec
		vector<Point> point_vec_temp = point_vec;
		point_vec.clear();
		for (int i = i_longest; i <= i_longest + L_longest - 1; ++i) {
			point_vec.push_back(point_vec_temp[i]);
		}
	}

}

// Check that the path exists

bool validate_path(string& userpath) {
	struct stat info;
	return stat(userpath.c_str(), &info) == 0;
}

bool makePath(string userpath) {

	// Try to make the path
	try {
		boost::filesystem::create_directories(userpath);
	}
	catch (exception e1){
		return false;
	}
	
	// Execute a controlled exit if the path does not exist
	if (!boost::filesystem::is_directory(userpath)) {
		return false;
	}

	// Check that the path exists now (including if it was already there)
	return boost::filesystem::is_directory(userpath);

}

vector<string> getFilesInPath(string path, string fileext) {

	// Assemble a vector of strings holding all the file names in this folder
	string fullfile;
	vector<string> allFullFiles;

	DIR *dir;
	struct dirent *ent;
	if ((dir = opendir(path.c_str())) != NULL) {


		while ((ent = readdir(dir)) != NULL) {

			// Assemble the full file path to the video
			fullfile = path;
			fullfile.append(ent->d_name);

			// Check if this is a valid avi file
			size_t f = fullfile.find(fileext);

			// Add to the list
			if (f<fullfile.npos) { allFullFiles.push_back(fullfile); }

		}
		closedir(dir);
	}
	else {
		/* could not open directory, return empty string */
		printf("ERROR: Failed to open directory:\n%s\n", path.c_str());
		system("PAUSE");
		allFullFiles.push_back(string(""));
	}

	// Warn user if no videos found
	controlledExit(!allFullFiles.empty(), "No video files found in directory:\n" + path);
	return allFullFiles;
}


// Get the current time as either a string or numeric coefficients
//		* ms = true --> include ms in the time string
//		* round_to_s = true --> round seconds to the nearest second and set ms to 0
string getCurrentTimeString(vector<double> &time_vals, bool ms, bool round_to_s) {

	// Get system time
	std::time_t t = std::time(0);   // get time now
	std::tm* now = std::localtime(&t);
	SYSTEMTIME lt;
	GetLocalTime(&lt);

	// Round if requested, but never round up when on 59 bc you get 60s
	if (round_to_s && lt.wMilliseconds > 499 && now->tm_sec != 59)
		now->tm_sec++;

	if (round_to_s)
		lt.wMilliseconds = 0;

	// Generate the string format, excluding milliseconds
	char temp[500] = "";

	sprintf(temp, "%0.4d-%0.2d-%0.2d (%.2d-%.2d-%.2d)", now->tm_year + 1900
		, now->tm_mon + 1
		, now->tm_mday
		, now->tm_hour
		, now->tm_min
		, now->tm_sec
	);

	// Populate the numeric vector version, including millisectons
	time_vals[0] = now->tm_year + 1900;
	time_vals[1] = now->tm_mon + 1;
	time_vals[2] = now->tm_mday;
	time_vals[3] = now->tm_hour;
	time_vals[4] = now->tm_min;
	time_vals[5] = now->tm_sec;
	time_vals[6] = lt.wMilliseconds;


	string time_str = temp;
	if (ms)
		time_str = time_str + "-" + to_string((int)lt.wMilliseconds);

	return time_str;
}

// Get just the time string, no coefficients
string getCurrentTimeString(bool ms, bool round_to_s) {

	// Seed time_vals
	vector<double> time_vals;
	for (int i = 0; i < 7; i++)
		time_vals.push_back(0);

	// Return string, discarding time_vals
	return getCurrentTimeString(time_vals, ms, round_to_s);

}

// Get the current date, a subset of the current time
string getCurrentDateString() {

	// Seed time_vals
	vector<double> time_vals;
	for (int i = 0; i < 7; i++)
		time_vals.push_back(0);

	// Get system time
	string temp = getCurrentTimeString(time_vals);
	return temp.substr(0, 10);

}

double getCurrentHour() {

	// Seed time_vals
	vector<double> time_vals;
	for (int i = 0; i < 7; i++)
		time_vals.push_back(0);

	// Get system time
	getCurrentTimeString(time_vals);

	// Return result
	return time_vals[3];
}

double getCurrentMinute() {

	// Seed time_vals
	vector<double> time_vals;
	for (int i = 0; i < 7; i++)
		time_vals.push_back(0);

	// Get system time
	getCurrentTimeString(time_vals);

	// Return result
	return time_vals[4];
}

double getCurrentSecond() {

	// Seed time_vals
	vector<double> time_vals;
	for (int i = 0; i < 7; i++)
		time_vals.push_back(0);

	// Get system time
	getCurrentTimeString(time_vals);

	// Return result
	return time_vals[5];
}


// Execute an external EXE file,  get output
// https://stackoverflow.com/questions/478898/how-to-execute-a-command-and-get-output-of-command-within-c-using-posix
//
// If you don't need the output, or if you want to return immediately, you can use system instead
// https://stackoverflow.com/questions/6962156/is-there-a-way-to-not-wait-for-a-system-command-to-finish-in-c

std::string executeExternal(const char* cmd) {

	std::array<char, 128> buffer;
	std::string result;
	std::shared_ptr<FILE> pipe(_popen(cmd, "r"), _pclose);
	if (!pipe) {
		return "NOT FOUND";
	}
	while (!feof(pipe.get())) {
		if (fgets(buffer.data(), 128, pipe.get()) != nullptr)
			result += buffer.data();
	}
	return result;

}


// Print an error message to both the cmd window and a debugging log, along with the time stamp.
// Optionally email the alert if the fnames are both supplied (default empty if not).
void printTimedError(string msg, ofstream& debug_log, string fname_email_program, string fname_email_alert) {

	// Prepend timestamp to the message
	msg = getCurrentTimeString(false) + "    " + msg;

	// Print the message to the command line AND the debug file
	cout << msg;
	debug_log << msg << endl;
	debug_log.flush();

	// If requested, email the user (depending on the settings in the executable, might not email every time)
	if (!fname_email_program.empty() && !fname_email_alert.empty()) {

		// Print error to the email alert file (overwriting existing)
		ofstream outfile(fname_email_alert);
		outfile << msg;
		outfile.close();
		boost_sleep_ms(600);

		// Run email engine
		string result = executeExternal(fname_email_program.c_str());


	}
}

// Helper (the actual thread function) for the deadline timer below. Not in header file. 
void printTimedErrorAyDeadlineHelper(string msg, ofstream& debug_log, string fname_email_program, string fname_email_alert,
									int deadline_time_s, bool *cancel)
{
	// Create timer and wait for deadline or cancel
	AnthonysTimer Td;
	boost_interrupt_point();
	while (Td.getElapsedTime() < deadline_time_s) {
		boost_sleep_ms(250);
		boost_interrupt_point();
	}


	// If cancel is still false, print and email error
	if (!*cancel)
		printTimedError(msg, debug_log, fname_email_program, fname_email_alert);

}


/* 
	This function creates a boost thread to wait for the deadline and returns immediately. It will run once (no loops built in)
	
	if *cancel is set to "true" before the deadline time runs out, the thread is destroyed WITHOUT printing or emailing any error.
	If *cancel is still "false" at the end of the deadline, the error in msg is printed and emailed.

	WARNING:
	It's very easy for cancel to go out of scope or be deallocated during program shutdowns or processes that finish faster
	than the deadline. Avoid using this method.
*/
void printTimedErrorAtDeadline(string msg, ofstream& debug_log, string fname_email_program, string fname_email_alert,
								int deadline_time_s, bool *cancel)
{
	// Create boost thread that lasts until deadline_time_s or until cancel is true
	*cancel = false; /// Make sure cancel is false. 
	boost::thread t1(&printTimedErrorAyDeadlineHelper, msg, boost::ref(debug_log), fname_email_program, fname_email_alert, deadline_time_s, cancel);

	// Return immediately. User is responsible for ending the thread by calling cancelTimedError(...);
}


// Return true if the vector of strings vs contains a string s any where
bool vsContains(vector<string> vs, string s) {

	bool test = std::find(vs.begin(), vs.end(), s) != vs.end();
	return test;
}

// calculate duration in appropriate units
std::string durationString(int seconds,std::string suffix) {

	string str;
	if (seconds < 0)
		str = "calculating time...";

	else if (seconds < 60)
		str = to_string(seconds) + " s";

	else if (seconds < 3600)
		str = to_string((int) ceil((double)seconds / 60)) + " min" + suffix;

	else if (seconds < 3600 * 24)
		str = to_string((int) ceil((double)seconds / 3600)) + " hr" + suffix;

	else
		str = to_string((int) ceil((double)seconds / 3600 / 24)) + " days" + suffix;

	return str;
}


// Extrapolate a vector
// in_vec: input vector
// out_vec: output vector
// up_lim: upper limit for extrapolation
// low_lim: lower limit for extrapolation, default 0 
void VecExtrapolate(vector<int>& in_vec, vector<int>& out_vec, int up_lim, int low_lim) {


	vector<int> orignal_vec(in_vec);


	// Sort the vector from small to large
	sort(orignal_vec.begin(), orignal_vec.end());
	out_vec.clear();
	out_vec.assign(orignal_vec.begin(), orignal_vec.end()); // copy to output vector

	// Get the adjcent differences
	vector<int> diff_vec;
	AdjDifference(orignal_vec, diff_vec);


	// Compute mean step size
	int step_size = vectorMean(diff_vec);

	// extrapolate towards the lower range, stop if smaller than the lower limit
	while (out_vec.front() - step_size >= low_lim) {

		//cout << "smallest element before extrap = " << out_vec.front() << endl;

		int smallest = out_vec.front() - step_size;
		out_vec.insert(out_vec.begin(), smallest);

		//cout << "smallest element after extrap = " << out_vec.front() << endl;
	}

	// extrapolate towards the upper range, stop if larger than the upper limit
	while (out_vec.back() + step_size <= up_lim) {

		//cout << "largest element before extrap = " << out_vec.back() << endl;

		int largest = out_vec.back() + step_size;
		out_vec.insert(out_vec.end(), largest);

		//cout << "largest element after extrap = " << out_vec.back() << endl;
	}

	return;
}


// Compute y value for a point on a line, given its x value
// line_vec: a vector of 4 elements - (vx, vy, x0, y0), 
// where(vx, vy) is a normalized vector collinear to the line and (x0, y0) is a point on the line.
int getlineYfromX(cv::Vec4f line_vec, int X) {

	float vx = line_vec[0];
	float vy = line_vec[1];
	float x0 = line_vec[2];
	float y0 = line_vec[3];

	if (vx == 0) {

		// it is a strictly vertical line, Y vlaue for the given X not defined!
		cout << "Y vlaue for the given X not defined!" << endl;
		return -1;
	}

	return ((float(X) - x0) * vy / vx) + y0;
}


// Compute x value for a point on a line, given its y value
// line_vec: a vector of 4 elements - (vx, vy, x0, y0), 
// where(vx, vy) is a normalized vector collinear to the line and (x0, y0) is a point on the line.
int getlineXfromY(cv::Vec4f line_vec, int Y) {

	float vx = line_vec[0];
	float vy = line_vec[1];
	float x0 = line_vec[2];
	float y0 = line_vec[3];


	if (vy == 0) {

		// it is a strictly horizontal line, X vlaue for the given Y not defined!
		cout << "X vlaue for the given Y not defined!" << endl;
		return -1;
	}


	return ((float(Y) - y0) * vx / vy) + x0;
}



// Compute differences between the adjcent elements in the vector
// vec_in: input vector having N elements, N >= 2 (X0, X1, X2, X3, ...)
// vec_out: output vector having N-1 elements (X1-X0, X2-X1, X3-X2, ...)
template <class T>
void AdjDifference(std::vector<T>& vec_in, std::vector<T>& vec_out) {

	vec_out.clear();
	if (size(vec_in) >= 2 ) {
		vector<T> shift_vec;
		shift_vec.assign(vec_in.begin(), vec_in.end());
		shift_vec.erase(shift_vec.begin());

		for (int i = 0; i < size(shift_vec); ++i) {
			vec_out.push_back(shift_vec[i] - vec_in[i]);
		}


	}
	else {
		cout << "Cannot compute adjcent differences, too few elements (<2) in the vector!" << endl;
	}
}


// Points shift by a certain offset in X and Y direction: dX and dY
void PointsOffset(std::vector<cv::Point>& pts_vec_in, std::vector<cv::Point>& pts_vec_out, int dX, int dY) {
	pts_vec_out.clear();

	for (int i = 0; i < size(pts_vec_in); ++i) {
		pts_vec_out.push_back(Point(pts_vec_in[i].x + dX, pts_vec_in[i].y + dY));
	}
}
