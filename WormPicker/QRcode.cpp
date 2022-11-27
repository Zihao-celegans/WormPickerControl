 /*
	QRcode.cpp
	Anthony Fouad
	Fang-Yen Lab
	January 2019

	DEFUNCT VERSION OF QR CODE TO BE REPLACED LATER
 */

#include "QRcode.h"
#include "ImageFuncs.h"
#include <vector>
#include <string>

 // OpenCV includes
#include "opencv2\opencv.hpp"

// Namespaces
using namespace std;
using namespace cv;

// Name a structure for holding found QR codes
typedef struct
{
	std::string type;
	std::string data;
	std::vector <cv::Point> location;
} decodedObject;



// Convert the QR object bounding box to a vector of points
vector<Point> QRcode::convertQrBbox(Mat &bbox, int shift_x, int shift_y){

	 int n = bbox.rows;
	 std::vector<cv::Point> out;
	 for (int i = 0; i < n; i++)
		 out.push_back(cv::Point(bbox.at<float>(i, 0) + shift_x, bbox.at<float>(i, 1) + shift_y));
	 return out;

}


bool QRcode::scanAndDecode(cv::Mat imggray) {

// Clear old codes
	location_determined = false;
	curr_qr_loc = MatIndex(-1, -1);
	all_qr_loc.clear();
	all_bounding_box.clear();
	all_decoded_data.clear();


// Isolate area of image containing label

	// get average and STD image intensity
	Scalar mean_val(0, 0, 0), stdev_val(0, 0, 0);
	meanStdDev(imggray, mean_val, stdev_val);
	double mean_d = mean_val[0];
	double stdev_d = stdev_val[0];

	//Threshold pixels below mean+1 stdev
	double thresh = min(mean_d + 4 * stdev_d, 240.0);
	Mat BW = imggray > thresh;

	// Remove small objects
	Mat kernel = getStructuringElement(MORPH_RECT, Size(9, 9));
	erode(BW, BW, kernel);

	// Get outer contour of remaining objects, which should be label stickers
	vector<vector<Point>> contours;
	vector<Point> bboxv;
	Rect bounding_rect;
	findContours(BW, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

// Search for QR codes inside extracted stickers
	Mat roihp, roilp, roiqr;

	for (int i = contours.size()-1; i >=0; i--) {

		// Check that contour is a valid sticker. If it's too small pop it out
		if (contourArea(contours[i]) < 99999) {
			contours.pop_back();
			continue;
		}

		// Calculate bounding rectangle
		bounding_rect = boundingRect(contours[i]);

		// Cut length and width by 15% to focus in on code
		double W_cut = 0.15 * bounding_rect.width;
		double x_shift = W_cut/2;
		double H_cut = 0.15 * bounding_rect.height;
		double y_shift = H_cut/2;

		bounding_rect.width -= W_cut;
		bounding_rect.height -= H_cut;
		bounding_rect.x += x_shift;
		bounding_rect.y += y_shift;
				
		// Work with image ROI
		Mat roiimg = imggray(bounding_rect);

	//	// Isolate sub-area of the sticker ROI containing QR code (high pass filterable)
	//	GaussianBlur(roiimg, roilp, Size(7, 7), 5);		/// Blur to get low pass image
	//	roihp = roiimg - roilp;							/// Subtract low pass from image to get high pass
	//	roihp = roihp > 1;								/// Threshold to get binary
	//	isolateLargestBlob(roihp, 3);					/// Get largest high frequency object - the QR code
	//	kernel = getStructuringElement(MORPH_RECT, Size(13,13));
	//	dilate(roihp, roihp, kernel);					/// Substantially dilate it so we don't cut anything off
	//	vector<vector<Point>> innercontours;
	//	findContours(roihp, innercontours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE); /// Find expanded contours and get bounding rect
	//	Rect qrbound = boundingRect(innercontours[0]); /// Note contours should always be exactly one entry
	//	roiqr = roiimg(qrbound);

	//	// Find QR code
	//	/*
	//		QRCodeDetector qrDecoder = QRCodeDetector::QRCodeDetector();
	//		Mat bbox;
	//		std::string data = qrDecoder.detectAndDecode(roiqr, bbox);
	//	*/
	//	std::string data = "Row: 00, Col:00";

	//	// If any QR code was found and interpereted, store its location
	//	if (data.length() > 0){
	//		//bboxv = convertQrBbox(bbox,bounding_rect.x+qrbound.x,bounding_rect.y+qrbound.y);
	//		//all_bounding_box.push_back(bboxv);
	//		//all_decoded_data.push_back(data);
	//		//all_qr_loc.push_back(parseQrRowCol(data));
	//	}
	}

// For all QR codes found: check if they agree and are not -1, -1

	if (all_qr_loc.size() > 0) {
		location_determined = true;
		MatIndex testidx = all_qr_loc[0];
		for (int i = 1; i < all_qr_loc.size(); i++) {

			// If multiple QR codes were read (e.g. not -1,-1) AND they don't match, mark false. 
			if ((testidx.row != all_qr_loc[i].row || testidx.col != all_qr_loc[i].col) && testidx.row > -1)
				location_determined = false;
		}
	}
	else {
		location_determined = false;
	}


// If found and agree and strings don't read -1,-1, mark success
	if (location_determined) {
		curr_qr_loc = all_qr_loc[0];
		return location_determined;
	}
	else {
		curr_qr_loc = MatIndex(-1, -1);
		all_qr_loc.clear();
		all_bounding_box.clear();
		all_decoded_data.clear();
		return location_determined;
	}
}

MatIndex QRcode::parseQrRowCol(string data) {

	// Find all numbers in the data string
	MatIndex retval(-1, -1);
	vector<double> all_nums =  string2double(data);

	// There should be exactly two parsed numbers from the four digits, if not, exit with invalid value
	if (all_nums.size() != 2) 
		return retval;
	
	// Put together first two and second two digits to form rows and cols
	retval.row = all_nums[0];
	retval.col = all_nums[1];

	// Return
	return retval;
}