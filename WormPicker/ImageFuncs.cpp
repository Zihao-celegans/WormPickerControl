/*
	ImageFuncs.h
	Anthony Fouad
	Fang-Yen Lab
	Updated July 2019

	Functions for basic acquisition, segmentation of images shared between WormPicker and WormWatcher
	NO FUNCTIONS THAT USE WORMPICKER SPECIFIC CLASSES
*/

#include "ImageFuncs.h"
#include "AnthonysKeyCodes.h"
#include "ThreadFuncs.h"

#include "boost/algorithm/string.hpp"
//#include "showDebugImage.h"

using namespace cv;
using namespace std;








/*
   Draws a cross at the coordinate ctr.
 */
void drawCross(Mat &img, Point ctr, Scalar color, int thickness, int radius) {

	// Draw the vertical line in the cross
	Point pt1 = Point(ctr.x, ctr.y + radius);
	Point pt2 = Point(ctr.x, ctr.y - radius);
	line(img, pt1, pt2, color, thickness, 8, 0);

	//Draw the horizontal line in the cross
	pt1 = Point(ctr.x + radius, ctr.y);
	pt2 = Point(ctr.x - radius, ctr.y);
	line(img, pt1, pt2, color, thickness, 8, 0);

}

/*
   Draws crosses for an vector of ctr points
*/
void drawCrossesForPoints(cv::Mat &img, std::vector<cv::Point> ctrs, cv::Scalar color, int thickness, int radius) {

	for (int i = 0; i < size(ctrs); ++i) {
		drawCross(img, ctrs[i], color, thickness, radius);
	}
}

/*
	Draw a plot on the image from a vector of cv::Point's
*/

void drawPlot(Mat &img, vector<Point> pts, Scalar color, int thickness, bool closed) {

	// Minimum 2 points
	if (pts.size() < 2)
		return;

	for (int i = 0; i < pts.size()-1; i++) {
		line(img, pts[i], pts[i + 1], color, thickness);
	}

	// Close if requested
	if (closed)
		line(img, pts[pts.size() - 1], pts[0], color, thickness);
}


double correlation(Mat img1, Mat img2) {

	// convert data-type to "float" -- ASSUME ALREADY DONE!!!!!!
	//cv::Mat im_float_1;
	//img1.convertTo(im_float_1, CV_32F);
	//cv::Mat im_float_2;
	//img2.convertTo(im_float_2, CV_32F);

	int n_pixels = img1.rows * img1.cols;

	// Compute mean and standard deviation of both images
	cv::Scalar im1_Mean, im1_Std, im2_Mean, im2_Std;
	meanStdDev(img1, im1_Mean, im1_Std);
	meanStdDev(img2, im2_Mean, im2_Std);

	// Compute covariance and correlation coefficient
	double covar = (img1 - im1_Mean).dot(img2 - im2_Mean) / n_pixels;
	double correl = covar / (im1_Std[0] * im2_Std[0]);

	return correl;
}




/*
	User enters an N digit number.
	Wait time is the wait time at the last digit. Zero wait is strongly recommended
*/

int waitKeyNum(int N, int wait_time, cv::Mat img_display, cv::Point num_loc, string fig_name) {

	// Create a copy of the display image
	Mat img_copy;
	img_display.copyTo(img_copy);

	// Declare variables
	vector<int> keys;
	int k = 0;

	// Get all digits
	for (int i = 0; i < N; i++) {

		// Get keypress
		k = waitKey(0);

		// Did the user press SPACE or ENTER? That mean's we've finished early
		if (k == keySpace || k == keyDelete || k == keyDel || k == keyBackSpace || k == keyEnter) {
			N = i;
			break;
		}

		// If not add the digit specified by (k-48) to the list
		keys.push_back(k - 48);

		// Are additional digits needed? Update user display
		img_copy.copyTo(img_display);
		if(i==N-1)
			putText(img_display, to_string(vectorNum(keys)), num_loc, 1, 1.5, colors::rr, 3);
		else
			putText(img_display, to_string(vectorNum(keys)) + "_", num_loc, 1, 1.5, colors::rr, 3);

		imshow(fig_name, img_display);
	}

	// Multiply by 10's
	return vectorNum(keys);
}

// Convert a vector of digits into a single number
int vectorNum(std::vector<int> v) {
	int N = (int) v.size();
	int total = 0;
	for (int i = 0; i < N; i++) {
		int exp = N - i - 1;
		total += (int) pow(10, exp) * v[i];
	}
	return total;
}

/*
	Filter binary objects by size, count them up and get their positions
	https://gist.github.com/JinpengLI/2cf814fe25222c645dd04e04be4de5a6
*/

int filterObjects(const Mat& bwin, Mat& bwout, int min_size, int max_size) {

	// Get a list of all objects
	Mat labels, stats, centroids;
	connectedComponentsWithStats(bwin, labels, stats, centroids);

	// Cycle through list and identify objects to keep, starting at 1 because 0 is background
	bwout = Mat::zeros(bwin.size(), CV_8U);
	int N_objs = 0;
	for (int i = 1; i < stats.rows; i++)
	{
		int A = stats.at<int>(i,CC_STAT_AREA);
		if (A >= min_size && A <= max_size) {
			Mat to_draw = labels == i;
			cv::bitwise_or(bwout, to_draw, bwout);
			N_objs++;
		}
	}

	return N_objs;
}

/*
	Filter binary objects by shape (Length to Width ratio) and count them up. No further analysis or output for now
*/

int filterObjectsLWRatio(const Mat& bwin, Mat& bwout, double LWR_min, double LWR_max, double L_min, double L_max) {

	// Label connected components using contours, an array of points
	vector<vector<Point>> contoursTemp, contoursOut;
	vector<Vec4i> hierarchy;
	findContours(bwin, contoursTemp, hierarchy, RETR_CCOMP, CHAIN_APPROX_SIMPLE);

	// Keep only the highest level contours (outer contours) matching the parameters
	contoursOut.clear();

	// Draw the objects on a blank canvas
	bwout = Mat::zeros(Size(bwin.cols, bwin.rows), CV_8U);

	for (int i = 0; i < contoursTemp.size(); i++) {

		double A = contourArea(contoursTemp[i]);		// Area
		double P = arcLength(contoursTemp[i], true);	// Perimeter
		double L = P / 2;								// Approximate length 
		double W = A / L;								// Approximate width
		double LWR = L / W;								// Length-to-Width ratio

		if (hierarchy[i][3] == -1 && L > L_min && L < L_max && LWR > LWR_min && LWR < LWR_max) {
			contoursOut.push_back(contoursTemp[i]);
			drawContours(bwout, contoursTemp, i, colors::ww, -1);
		}
	}



	return contoursOut.size();
}

// https://stackoverflow.com/questions/18697532/gaussian-filtering-a-image-with-nan-in-python
Mat gaussFiltExclZeros(Mat U, int ksize, double sig) {

	// Create a copy of the image
	Mat V, VV, W, WW, Z;
	U.copyTo(V);
	U.copyTo(W);

	// Blur the original image
	GaussianBlur(V, VV, Size(ksize, ksize), sig);

	// Blur a ones image??? Don't really understand how this works...
	W = 0 * W + 1;
	GaussianBlur(W, WW, Size(ksize, ksize), sig);

	Z = VV / WW;
	return Z;
}


/*
	Remove blobs touching the border of an image

*/

void filterObjectsTouchingBorder(const cv::Mat& bwin, cv::Mat& bwout) {

	// Label connected components using contours, an array of points
	vector<vector<Point>> contoursTemp, contoursOut;
	vector<Vec4i> hierarchy;
	findContours(bwin, contoursTemp, hierarchy, RETR_CCOMP, CHAIN_APPROX_SIMPLE);

	// Keep only the highest level contours (outer contours) matching the parameters
	contoursOut.clear();

	// Draw the objects on a blank canvas
	bwout = Mat::zeros(Size(bwin.cols, bwin.rows), CV_8U);

	for (int i = 0; i < contoursTemp.size(); i++) {

		if (!contourTouchesImageBorder(contoursTemp[i],Size(bwin.cols, bwin.rows))) {
			contoursOut.push_back(contoursTemp[i]);
			drawContours(bwout, contoursTemp, i, colors::ww, -1);
		}
	}
}



/*
	Determine whether a contour's bounding box touches the boundaries of an image
	https://stackoverflow.com/questions/40615515/how-to-ignore-remove-contours-that-touch-the-image-boundaries
*/


bool contourTouchesImageBorder(std::vector<cv::Point>& contour, cv::Size& imageSize)
{
	cv::Rect bb = cv::boundingRect(contour);

	bool retval = false;

	int xMin, xMax, yMin, yMax;

	xMin = 0;
	yMin = 0;
	xMax = imageSize.width - 1;
	yMax = imageSize.height - 1;

	// Use less/greater comparisons to potentially support contours outside of 
	// image coordinates, possible future workarounds with cv::copyMakeBorder where
	// contour coordinates may be shifted and just to be safe.
	if (bb.x <= xMin ||
		bb.y <= yMin ||
		bb.x+bb.width >= xMax ||
		bb.y+bb.height >= yMax)
	{
		retval = true;
	}

	return retval;
}


/*
	Find the type of an openCV image as a string
	https://stackoverflow.com/questions/10167534/how-to-find-out-what-type-of-a-mat-object-is-with-mattype-in-opencv
*/

string cvtype2str(int type) {
	string r;

	uchar depth = type & CV_MAT_DEPTH_MASK;
	uchar chans = 1 + (type >> CV_CN_SHIFT);

	switch (depth) {
	case CV_8U:  r = "8U"; break;
	case CV_8S:  r = "8S"; break;
	case CV_16U: r = "16U"; break;
	case CV_16S: r = "16S"; break;
	case CV_32S: r = "32S"; break;
	case CV_32F: r = "32F"; break;
	case CV_64F: r = "64F"; break;
	default:     r = "User"; break;
	}

	r += "C";
	r += (chans + '0');

	return r;
}

// Rotate an image arbitrary angle, with or without clipping OOB
// https://stackoverflow.com/questions/14870089/how-to-imrotate-with-opencv-2-4-3
Mat rotateImage(const Mat source, double angle, int border)
{
	/// Create border if non-clipping requested
	Mat bordered_source;
	if (border != 0){
		int top, bottom, left, right;
		top = bottom = left = right = border;
		copyMakeBorder(source, bordered_source, top, bottom, left, right, BORDER_CONSTANT, cv::Scalar());
	}
	else {
		source.copyTo(bordered_source);
	}
	
	/// Get rotation matrix and warp affine the image
	Point2f src_center(bordered_source.cols / 2.0F, bordered_source.rows / 2.0F);
	Mat rot_mat = getRotationMatrix2D(src_center, angle, 1.0);
	Mat dst;
	warpAffine(bordered_source, dst, rot_mat, bordered_source.size());

	/// Return image
	return dst;
}

Point centroidOfPoints(vector<Point> pts) {

	Point out(0, 0);
	
	// xbar
	double xbar = 0;
	for (int i = 0; i < pts.size(); i++) {
		xbar += pts[i].x;
	}
	out.x = xbar / pts.size();

	//ybar
	double ybar = 0;
	for (int i = 0; i < pts.size(); i++) {
		ybar += pts[i].y;
	}
	out.y = ybar / pts.size();

	return out;
}


// Get centroids of each contours in the images
void centroidOfContours(vector<vector<Point>> contours, vector<Point>& out_centroids) {

	// clear the output array of centroids first
	out_centroids.clear();

	// Get the centroid of each contour and append to the output vector
	for (int i = 0; i < size(contours); ++i) {
		out_centroids.push_back(centroidOfPoints(contours[i]));
	}
}


// Take the mean of CV_32F images (only)

Mat meanOfImages(const deque<Mat>& images, int i0, int i1, int skip) {

	// Create return image
	Mat sum_image = Mat::zeros(images[0].size(), CV_32F);
	Mat mean_image = Mat::zeros(images[0].size(), CV_32F);


	// Enforce i0 and i1 in range and i1 > 0
	i1 = std::min(i1, (int) images.size() - 1);

	if (i1 == 0 || i0 >= i1 || i0 < 0 || images.size() == 0) {
		printf("ERROR: Failed to take mean of images\n");
		return mean_image;
	}
	int N_images = 0;
	for (int i = i0; i <= i1; i+=skip) {

		sum_image += images[i];
		N_images++;
	}

	mean_image = sum_image / N_images;
	return mean_image;
}

// Take minimum of images (Can be any kind)
Mat minOfImages(const deque<Mat>& images, int i0, int i1, int skip) {

	// Create return image
	Mat min_image = Mat::ones(images[0].size(), images[0].type()) * 9999;

	// Enforce i0 and i1 in range and i1 > 0
	i1 = std::min(i1, (int)images.size() - 1);

	if (i1 == 0 || i0 >= i1 || i0 < 0 || images.size() == 0 || skip >= images.size()) {
		printf("ERROR: Failed to take minimum of images\n");
		return min_image;
	}

	for (int i = i0; i <= i1; i+=skip) {
		min_image  = cv::min(min_image,images[i]);
	}

	return min_image;
}

/*
	Find the darkest N pixels in imgin (A CV_32F IMAGE) and set them as 1.0 in imgout.
	Optionally limit searches to within a certain mask (improves speed)
*/
void segmentDarkestN(const Mat& imgin, Mat &imgout, int N, Mat mask) {

	// Prepare imgout and enforce valid size
	imgout = Mat::zeros(imgin.size(), CV_8U);

	if (imgin.rows * imgin.cols < N) {
		return;
	}

	// Enforce 32F
	if (imgin.type() != CV_32F) {
		cout << "ERROR: segmentDarkestN requires 32F imgin" << endl;
		return;
	}

	// Enforce CV_8U mask
	if (mask.type() != CV_8U) {
		cout << "ERROR: segmentDarkestN requires 8U imgout" << endl;
		return;
	}

	// If the default mask was used, set it to the whole image
	if (mask.rows == 1) {
		mask = Mat::ones(imgin.size(), CV_8U) * 255;
	}

	// Pass 1: sort all pixels in the mask by order of intensity
	//will be used to calculate the threshold intensity later
	vector<float> intensities;

	for (int i = 0; i < imgin.rows; i++) {
		for (int j = 0; j < imgin.cols; j++) {
			if (mask.at<uchar>(i, j) != 0) {
				intensities.push_back(imgin.at<float>(i, j));
			}
		}
	}

	sort(intensities.begin(), intensities.end(), less<float>());

	// Find the intensity of the top 100
	double thresh = intensities[N];

	// Pass 2: Keep the pixels within the mask that have intensity > thresh
	for (int i = 0; i < imgin.rows; i++) {
		for (int j = 0; j < imgin.cols; j++) {
			if (mask.at<uchar>(i, j) != 0 && imgin.at<float>(i, j) <= thresh) {
				imgout.at<uchar>(i, j) = 255;
			}
		}
	}
}


// Paint over all binary objects except the largest one (by contour area)

void isolateLargestBlob(Mat &bw, int erode_size) {

	// Filter out small objects
	if (erode_size >= 3) {
		Mat kernel = getStructuringElement(MORPH_RECT, Size(erode_size, erode_size));
		erode(bw, bw, kernel);					/// remove crap noise
		dilate(bw, bw, kernel);
	}

	// Get contours
	vector<vector<Point>> contours;
	findContours(bw, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

	// Find the contour with the largest area
	int max_area = 0, max_contour_num = 0;

	for (int i = 0; i < contours.size(); i++) {
		int A = (int)contourArea(contours[i]);
		if (A > max_area) {
			max_area = A;
			max_contour_num = i;
		}
	}

	// Set the original image to binary 0
	bw = 0;

	// Draw the contour
	drawContours(bw, contours, max_contour_num, colors::ww, -1);
}

/*
	Get largest object's contour in a BW imaging.
	Faster than isolateLargestObject if you only need the contour. 

	contour may be empty if none found
*/

std::vector<cv::Point> isolateLargestContour(const cv::Mat& bw, int min_contour_size) {

	// Get all contours
	vector<Point> contour; /// Final contour
	vector<vector<Point>> contoursTemp;
	vector<Vec4i> hierarchy;
	findContours(bw, contoursTemp, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

	// Select the largest contour, assuming any were found. If not fill contour with empty
	//		TODO: Select contour by size matching, or movement, or darkness, or CNN

	
	for (int i = 0; i < contoursTemp.size(); i++) {
		if (contoursTemp[i].size() > min_contour_size && contoursTemp[i].size() > contour.size())
			contour = contoursTemp[i];
	}

	return contour;
}


void findoverlapBlob(cv::Mat &ref_bw, cv::Mat &obj_bw) {

	// Get contours
	vector<vector<Point>> obj_contrs;
	findContours(obj_bw, obj_contrs, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

	// Declare a vector of contours to hold the contours of interest
	vector<vector<Point>> inter_contrs;

	// Find the overlaping contours
	for (int i = 0; i < obj_contrs.size(); i++) {

		// Intermediate image that contains a contour in obj_bw
		Mat this_contr = Mat::zeros(ref_bw.size(), CV_8UC1);
		this_contr = 0;
		drawContours(this_contr, obj_contrs, i, colors::ww, -1);

		// Judge whether this contour is overlapping with any contours in ref_bw
		Mat overlap;
		bitwise_and(this_contr, ref_bw, overlap);
		double min, max;
		cv::minMaxLoc(overlap, &min, &max);
		bool is_overlap = (max > 0);

		if (is_overlap) {
			inter_contrs.push_back(obj_contrs[i]);
		}
	}

	obj_bw = 0;
	// Draw the contour
	drawContours(obj_bw, inter_contrs, -1, colors::ww, -1);

	// Debug image

}

/*
	Adapted from:
	https://www.morethantechnical.com/2012/12/07/resampling-smoothing-and-interest-points-of-curves-via-css-in-opencv-w-code/
*/

bool ResampleCurve(std::vector<Point> original, std::vector<Point>& resampled, int N) {


	// If an empty original is supplied (or an original with less than 3 poin ts), return false
	if (original.size() < 3)
		return false;

	// Compute the arc length of the curve, and how much space should be in between each point after resampling
	double pl_length = arcLength(original, false);
	double resample_size = pl_length / (double)N;
	int curr = 0;
	double dist = 0.0;

	// Seed the resampled curve with first value from original
	resampled = vector<Point>(N);
	resampled[0] = original[0];

	// Compute remaining points according to the desired spacing, interpolate between nearst neighbors
	for (int i = 1; i < N && curr+1 < original.size(); ) {


		double last_dist = norm(original[curr] - original[curr + 1]);
		dist += last_dist;


		if (dist >= resample_size) {
			//put a point on line
			double _d = last_dist - (dist - resample_size);
			Point2d cp(original[curr].x, original[curr].y), cp1(original[curr + 1].x, original[curr + 1].y); /// Extract original and next original point
			Point2d dirv = cp1 - cp; dirv = dirv * (1.0 / norm(dirv)); /// Derivative between these points

			resampled[i] = cp + dirv * _d;
			i++;

			dist = last_dist - _d; //remaining dist         

			//if remaining dist to next point needs more sampling... (within some epsilon)
			while (dist - resample_size > 1e-3 && i < N) {

				resampled[i] = resampled[i - 1] + (Point) (dirv * resample_size);
				dist -= resample_size;
				i++;
			}
		}

		curr++;
	}
	return true;
}


// Get the maxima and minima of an interval of a vector
void getMinMax(const std::vector<double>& vec_in, double& min, int& loc_min, double& max, int& loc_max, double start, double end) {

	// Start & end location
	int start_idx = round(vec_in.size()*start);
	int end_idx = round(vec_in.size()*end);

	min = vec_in[start_idx];
	max = vec_in[start_idx];

	for (int i = start_idx; i < end_idx; i++) {

		if (vec_in[i] < min) {
			min = vec_in[i];
			loc_min = i;
		}

		if (vec_in[i] > max) {
			max = vec_in[i];
			loc_max = i;
		}
	}
}

// Get the convexity of an interval of a vector
// TO DO: change to general version: depends on the position of maximum and minimum
double getConvexity(const std::vector<double>& vec_in, double POI_start, double POI_end) {

	double min = -1, max = -1;
	int loc_min = -1, loc_max = -1;

	// Get the maximum and minimum of the portion
	getMinMax(vec_in, min, loc_min, max, loc_max, POI_start, POI_end);

	// Start & end of the interval
	int start_idx = round(vec_in.size()*POI_start);
	int end_idx = round(vec_in.size()*POI_end);

	// Copmute the area of shape bounded by the linear base line and intensity profile
	double line = -1;
	double diff = 0;

	for (int i = start_idx; i < end_idx; i++) {
		line = max - (i - start_idx)*(max - min) / ((end_idx-1) - start_idx);
		diff = diff + (vec_in[i] - line);
	}

	return diff;
}

// Returns the index of the column in the provided image that has the maximum average intensity 
// Or index of column of center of mass of the Intensity Distribution
int getPeakOrCenterOfMassOfIntensityDistribution(cv::Mat& img, bool want_Peak) {
	
	// Get the average value of each column in the image
	vector<int> intensity_sums(img.cols, 10);
	for (int row = 0; row < img.rows; ++row) {
		uchar* p = img.ptr(row);
		for (int col = 0; col < img.cols; ++col) {
			intensity_sums[col] += abs(*p);
			p++;
		}
	}

	vector<double> intensity_avgs;
	for (int col_sum : intensity_sums) {
		intensity_avgs.push_back(col_sum / intensity_sums.size());
	}

	// Get the index of the column with the maximum average intensity

	if (want_Peak) {
		int max_idx;
		vectorMax(intensity_avgs, &max_idx);
		return max_idx;
	}
	else {
		return CenterofMass(intensity_avgs);
	}

}

// Rudimentary function to get the area of the image with the largest intensity slope. Returns the index of the biggest slope.
int getCliffOfIntensityDistribution(cv::Mat& img) {

	// Get the average value of each column in the image
	vector<int> intensity_sums(img.cols, 10);
	for (int row = 0; row < img.rows; ++row) {
		uchar* p = img.ptr(row);
		for (int col = 0; col < img.cols; ++col) {
			intensity_sums[col] += abs(*p);
			p++;
		}
	}

	vector<double> intensity_avgs;
	for (int col_sum : intensity_sums) {
		intensity_avgs.push_back(col_sum / intensity_sums.size());
	}

	int sliding_window = 20;
	double biggest_drop = 0;
	int biggest_drop_idx = 0;
	for (int i = 0; i <= intensity_avgs.size() - sliding_window; i++) {
		//cout << "i = " << i << endl;
		int window_min = intensity_avgs[i];
		int window_max = intensity_avgs[i];
		int window_min_idx = i;
		int window_max_idx = i;
		for (int j = i; j < i + sliding_window; j++) {
			//cout << "\tj = " << j << ", " << intensity_avgs[j] << endl;
			if (intensity_avgs[j] < window_min) {
				window_min = intensity_avgs[j];
				window_min_idx = j;
			} 
			else if (intensity_avgs[j] > window_max) {
				window_max = intensity_avgs[j];
				window_max_idx = j;
			}
		}
		if(window_max - window_min >= biggest_drop){
			biggest_drop = window_max - window_min;
			biggest_drop_idx = (window_max_idx + window_min_idx) / 2; // We take the center of the drop.
		}

		/*cout << "\t\twindow_min, max = " << window_min << ", " << window_max << endl;
		cout << "\t\twindow_min idx, max idx = " << window_min_idx << ", " << window_max_idx << endl;
		cout << "\t\tbiggest drop, idx = " << biggest_drop << ", " << biggest_drop_idx << endl;*/
	}

	/*Mat debug;
	cvtColor(img, debug, CV_GRAY2RGB);
	Point top(biggest_drop_idx, 0);
	Point bottom(biggest_drop_idx, img.rows - 1);
	line(debug, top, bottom, colors::yy, 2);
	imshow("Cliff line", debug);
	waitKey(0);
	destroyWindow("Cliff line");*/

	return biggest_drop_idx;


}

// Get the ROIs from vector of center pts and prevent out of bound
void getROIsfromPts(vector<Point>& pts, Size full_sz, vector<Rect>& vec_Rois, int roi_radius) {

	for (int i = 0; i < pts.size(); i++) {

		// Set the ROI for each high energy centroid
		Rect ROI;
		ROI.x = pts[i].x - roi_radius;
		ROI.y = pts[i].y - roi_radius;
		ROI.width = 2 * roi_radius;
		ROI.height = 2 * roi_radius;

		// Prevent OOB
		ROI.x = max(ROI.x, 0);
		ROI.y = max(ROI.y, 0);
		ROI.width = min(ROI.width, full_sz.width - ROI.x);
		ROI.height = min(ROI.height, full_sz.height - ROI.y);

		vec_Rois.push_back(ROI);
	}
}

// Get groups of points lying in same veritical / horizontal line
// Group the points with similar x or y value i.e. the x or y differences less than a threshold
// Grid_pts: input vector of points
// groups_of_pts: output vector of points grouped by similar x or y coordinates
// sz_img: size of image 
// p_length_thresh: percentage threshold. E.g. 5% means the differences in x / y less than 5% of the image width / height are considered lying on same vertical / horizontal line
// find_horizontal: flag to indicate find points lying in horizontal (true) or vertical (false)
void getPointsAtSameVecHorztLine(std::vector<cv::Point>& Grid_pts, std::vector<std::vector<cv::Point>>& groups_of_pts, Size sz_img, double p_length_thresh, bool find_horizontal) {

	// set storing the index of elements that are already taken into calculation
	set<int> index_to_exclude;

	for (int i = 0; i < size(Grid_pts); i++) {

		// check whether the element is need to be excluded for searching
		if (setFind(index_to_exclude, i) == true) {
			continue;
		}

		//// Add the i to the index to exclude
		index_to_exclude.insert(i);

		// set storing the index of elements that are on the same vertical line
		set<int> idx_this_group_point;


		for (int j = i + 1; j < size(Grid_pts); j++) {

			// check whether the element is need to be excluded for searching
			if (setFind(index_to_exclude, j) == true) {
				continue;
			}

			// calculate x / y differences  of rest points to point[i]
			double this_diff = 0;
			bool on_same_vect_line = false;

			// two points are on the same line if their distance in x / y is less than the threshold
			if (find_horizontal){
				this_diff = std::abs(Grid_pts[i].y - Grid_pts[j].y);
				on_same_vect_line = ((this_diff / double(sz_img.height)) < p_length_thresh);
			}
			else{
				this_diff = std::abs(Grid_pts[i].x - Grid_pts[j].x);
				on_same_vect_line = ((this_diff / double(sz_img.width)) < p_length_thresh);
			}

			if (on_same_vect_line) {

				// add the index of two points to the set
				idx_this_group_point.insert(i);
				idx_this_group_point.insert(j);

				// add the index of j to exlcude list for excluding further search using this point
				index_to_exclude.insert(j);

			}
		}

		// Get the point based on the index
		vector<Point> this_vec_point;
		set<int>::iterator itr;

		// Get the point based on the index
		for (itr = idx_this_group_point.begin();
			itr != idx_this_group_point.end(); itr++)
		{
			this_vec_point.push_back(Grid_pts[*itr]);
		}


		// append the set of points to output
		groups_of_pts.push_back(this_vec_point);

	}


	return void();
}


// Extrapolate a 1D vector of grid points using linear extrapolation
// in_points: input vector of points
// out_points: output vector of points
// extrap_in_x: flag to indicate extrapolate using x values (true) or y vlaues (false)
// up_lim: upper x / y limit for extrapolation
// low_lim: lower x / y limit for extrapolation, default 0 (image edge)
void OneDimGridExtrapolate(vector<Point>& in_points, vector<Point>& out_points, bool extrap_in_x, int up_lim, int low_lim)
{

	if (size(in_points) < 2) { 
		out_points.clear();
		out_points.assign(in_points.begin(), in_points.end());
		cout << "Too few points (<2) for extrapolate!" << endl;
		return; 
	}


	// Fit a line for the existing points
	Vec4f line_vec;
	fitLine(in_points, line_vec, DIST_L2, 0, 0.01, 0.01);


	// Get the x / y coordinates from original input points
	vector<int> OneDimCoords;
	for (int i = 0; i < size(in_points); ++i) {
		if (extrap_in_x) OneDimCoords.push_back(in_points[i].x);
		else OneDimCoords.push_back(in_points[i].y);
	}

	// Vector extrapolation
	vector<int> OneDimCoords_extrap;
	VecExtrapolate(OneDimCoords, OneDimCoords_extrap, up_lim, low_lim);

	// Get
	// TODO: Can maintain the original x / y inside the known range

	out_points.clear();
	for (int i = 0; i < size(OneDimCoords_extrap); ++i) {

		if (extrap_in_x == true) { 
			int this_Y = getlineYfromX(line_vec, OneDimCoords_extrap[i]);
			Point this_point(OneDimCoords_extrap[i] , this_Y);
			out_points.push_back(this_point);
		}
		else {
			int this_X = getlineXfromY(line_vec, OneDimCoords_extrap[i]);
			Point this_point(this_X, OneDimCoords_extrap[i]);
			out_points.push_back(this_point);
		}
	}

}


void TwoDimGridExtrapolate(vector<Point>& Grid_pts, vector<Point>& extrap_Grid_pts, Size sz_img, double p_leng_thresh) {

	vector<vector<Point>> groups_vert_pts;
	vector<vector<Point>> groups_horiz_pts;
	vector<Point> extrap_grid_vect;

	// Get the vectical line for extrapolation
	getPointsAtSameVecHorztLine(Grid_pts, groups_vert_pts, sz_img, p_leng_thresh, false);

	// debug Image
	Mat debug = Mat::zeros(sz_img, CV_8UC3);

	for (int i = 0; i < size(groups_vert_pts); ++i) {

		vector<Point> pts_on_vert_line;
		OneDimGridExtrapolate(groups_vert_pts[i], pts_on_vert_line, false, sz_img.height - 1, 0);

		// Append the extrapolation
		for (int j = 0; j < size(pts_on_vert_line); ++j) {
			extrap_grid_vect.push_back(pts_on_vert_line[j]);
		}

	}


	// Get the horizontal line for extrapolation
	getPointsAtSameVecHorztLine(extrap_grid_vect, groups_horiz_pts, sz_img, p_leng_thresh, true);

	// extrapolate in horizontal, on top of the extrpolated points in veritical
	for (int i = 0; i < size(groups_horiz_pts); ++i) {

		vector<Point> pts_on_horiz_line;
		OneDimGridExtrapolate(groups_horiz_pts[i], pts_on_horiz_line, true, sz_img.width - 1, 0);

		// Append the extrapolation
		for (int j = 0; j < size(pts_on_horiz_line); ++j) {
			extrap_Grid_pts.push_back(pts_on_horiz_line[j]);
		}
	}

	/*drawCrossesForPoints(debug, extrap_grid_final, colors::rr, 2, 4);

	imshow("Extrap in vertical", debug);
	waitKey(1000);*/


}




