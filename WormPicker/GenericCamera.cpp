/*
	GenericCamera.cpp
	Alexander Kassouni
	Fang-Yen Lab
	Summer 2019

	Updated by Siming He
	Summer 2022

	A generic AbstractCamera implementation that uses opencv's VideoCapture
*/

#include "GenericCamera.h"

// Anthony's includes
#include "stdafx.h"
#include "AnthonysCalculations.h"
#include "ImageFuncs.h"
#include "MatIndex.h"
#include "SharedOutFile.h"
#include "ThreadFuncs.h"
#include "ErrorNotifier.h"
#include "QRcode.h"


// OpenCV includes
#include "opencv2\opencv.hpp"

// Standard includes
#include <vector>
#include <string>
#include <fstream>

// Boost includes
#include "boost\thread.hpp"

// Namespaces
using namespace std;
using namespace cv;

// Constructor: parameters specified as arguments
// when file is the empty string, load a camera
// otherwise, use the file name to load a recording
GenericCamera::GenericCamera(int n, Size as, Size gs, Size cs, double pm, bool pfh, bool pfv, AbstractCamera::camera_running_mode run_mode, string fileName)
	: AbstractCamera(n, as, gs, cs, pm)
{
	//Size res(2592, 1944);

	// Initialize camera running mode
	cam_run_mode = run_mode;

	// get live frame from camera
	if (cam_run_mode == LIVE) {
		stream0 = VideoCapture(cap_num);
		bool temp = stream0.isOpened();


		stream0.set(CAP_PROP_FRAME_HEIGHT, acquire_size.height);
		stream0.set(CAP_PROP_FRAME_WIDTH, acquire_size.width);

		// verify that camera or file was opened
		boost_sleep_ms(250);

		if (stream0.isOpened()) {
			printf("Connected to camera %d\n\n", cap_num);
		}
		else {
			printf("Attempted to use camera but failed to open camera %d\n\n", cap_num);
		}
	}
	// get live frame from local video file
	else if (cam_run_mode == READ_LOCAL) {
		stream0 = VideoCapture(fileName);
		// verify that camera or file was opened
		boost_sleep_ms(250);
		if (stream0.isOpened()) {
			cout << "Reading from video file:" << fileName << endl;
		}
		else {
			cout << "Could not load video file:" << fileName << endl;
		}
	}

	// get dummy (empty) frame
	else if (cam_run_mode == DUMMY){
		cout << "Grab frame from Genric camera in DUMMY mode! " << endl;
	}
	

	// Setup remaining parameters
	flip_horiz = pfh;
	flip_vert = pfv;

	// If no resizing (preferred size is negative), then set the acquire size as preferred size
	if (color_size.width < 0 || color_size.height < 0)
		color_size = acquire_size;

	if (gray_size.width < 0 || gray_size.height < 0) gray_size = acquire_size;

	// Establish empty images
	img_color = Mat(color_size.height, color_size.width, CV_8UC3, Scalar(0, 0, 0));
	img_gray = Mat(gray_size.height, gray_size.width, CV_8U, Scalar(0, 0, 0));
}

// Destructor - disconnects cameras automatically when stream0 object destructs
GenericCamera::~GenericCamera() {
	stream0.release();
}

// Reconnect (e.g. if connection fails)
bool GenericCamera::reconnect(StrictLock<AbstractCamera> &camLock) {

	// Wait at least 5 seconds before attempting to reopen in order to avoid the popup window prompt for camera selection
	boost_sleep_ms(5000);

	// Attempt to reconnect to the camera
	stream0.release();
	boost_sleep_ms(500);
	stream0.open(cap_num);
	stream0.set(CAP_PROP_FRAME_HEIGHT, acquire_size.height);
	stream0.set(CAP_PROP_FRAME_WIDTH, acquire_size.width);
	boost_sleep_ms(500);

	// Return camera open state
	return stream0.isOpened();

}

// Acquire image
bool GenericCamera::grabImage(StrictLock<AbstractCamera> &camLock) {
	
	// Matrix to store the acquired data
	Mat img_acq;

	bool valid_image_retrieved = false;


	// get live frame from camera
	if (cam_run_mode == LIVE) {
		// Attempt to grab an image
		stream0.read(img_acq);

		// Check whether a valid image was grabbed
		valid_image_retrieved = validateImage(img_acq);
	}
	// get frame from file
	else if (cam_run_mode == READ_LOCAL) {

		stream0.set(CAP_PROP_POS_MSEC, 0);
		stream0.read(img_acq);

		// Check whether a valid image was grabbed
		valid_image_retrieved = validateImage(img_acq);
		
	}

	// get dummy (empty) frame
	else if (cam_run_mode == DUMMY) {
		img_acq = Mat::zeros(acquire_size.height, acquire_size.width, CV_8UC3);
		valid_image_retrieved = true;
	}


	// Perform preprocessing and hand over image for use ONLY if image is requested 
	/// (as opposed to being used for buffer flushing)
	if (valid_image_retrieved) {

		// Adjust image multiplicitive gain
		if (preferredMean > 0) {
			double mulGain = preferredMean / mean(img_acq)[0];
			img_acq = img_acq * mulGain;
		}


		// To Do: Add the flag of frame rotation in the json file                                            
		//cv::rotate(img_acq, img_acq, cv::ROTATE_90_CLOCKWISE);


		// Flip horizontal axis (always on for offline mode to prevent mirror images)
		if (flip_horiz)
			flip(img_acq, img_acq, 1);

		if (flip_vert)
			flip(img_acq, img_acq, 0);

		// Hand image over for use and convert to grayscale
		img_acq.copyTo(img_color);
		cvtColor(img_acq, img_gray, COLOR_RGB2GRAY);
		// img_acq.copyTo(img_gray);

		//cvtColor(img_gray, img_color, COLOR_GRAY2RGB);

		// Resize images if specified 

		bool b5 = color_size.width > 0;
		bool b6 = color_size.height > 0;
		bool b7 = color_size.width != acquire_size.width;
		bool b8 = color_size.height != acquire_size.height;

		try {
			if (b5 && b6 && b7 && b8)
				/* if (color_size.width > 0 &&
					color_size.height > 0 &&
					color_size.width != acquire_size.width &&
					color_size.height != acquire_size.height) */
				resize(img_color, img_color, color_size);
		}
		catch (cv::Exception & e) {
			cout << "*****************ERROR******************" << endl;
			cerr << e.msg << endl;
			log_message = "ERROR: " + e.msg + " " + to_string(cap_num) + " at " + time_stamp;
		}

		/*cout << to_string(cap_num) << " Gray Width: " << gray_size.width << " " << (gray_size.width > 0) << endl;
		cout << to_string(cap_num) << " Gray height: " << gray_size.height << " " << (gray_size.height > 0) << endl;
		cout << to_string(cap_num) << " acquire Width: " << acquire_size.width << " " << (gray_size.width != acquire_size.width) << endl;
		cout << to_string(cap_num) << " acquire height: " << acquire_size.height << " " << (gray_size.height != acquire_size.height) << endl;
		*/


		bool b1 = gray_size.width > 0;
		bool b2 = gray_size.height > 0;
		bool b3 = gray_size.width != acquire_size.width;
		bool b4 = gray_size.height != acquire_size.height;

		try {
			if (b1 && b2 && b3 && b4)
			/* if (gray_size.width > 0 && gray_size.height > 0 &&
				gray_size.width != acquire_size.width &&
				gray_size.height != acquire_size.height) */
			{

				// cout << to_string(cap_num) << " Condition pass" << endl;
				resize(img_gray, img_gray, gray_size);
				// cout << "resized gray_img" << endl;
			}
		}
		catch (cv::Exception & e) {
			cout << "*****************ERROR******************" << endl;
			cerr << e.msg << endl;
			// Set an error message in the log
			log_message = "ERROR: " + e.msg + " " + to_string(cap_num) + " at " + time_stamp;
		}

			/*if (gray_size.width > 0 &&
				gray_size.height > 0 &&
				gray_size.width != acquire_size.width &&
				gray_size.height != acquire_size.height) 
			{

				// cout << to_string(cap_num) << " Condition pass" << endl;
				resize(img_gray, img_gray, gray_size);
			}*/


		log_message = "Image acquired at " + time_stamp + " on cam" + to_string(cap_num);
	}

	// If image was requested but is not valid (e.g. disconnect) set a dummy zero image with text label
	else {

		// Add disconnect message
		putText(img_color, string("Invalid image - is camera connected?"), Point(10, (int)(img_color.rows / 2)), 1, 1, colors::yy, 1);
		putText(img_gray, string("Invalid image - is camera connected?"), Point(10, (int)(img_gray.rows / 2)), 1, 1, colors::ww, 1);

		// Set an error message in the log
		log_message = "ERROR: No image returned from cam" + to_string(cap_num) + " at " + time_stamp;
	}

	// Return the validity state for external use
	return valid_image_retrieved;
}

cv::Mat GenericCamera::grabFullRes(cv::Rect roi, StrictLock<AbstractCamera> &camLock) {
	return Mat();
}

AbstractCamera::camera_make GenericCamera::getMake() {
	return AbstractCamera::GENERIC;
}

void GenericCamera::getCamHandle(StrictLock<AbstractCamera> &camLock, void*& CamHandle) {
	cout << "Dummy get generic camera handle" << endl;
	return void();
}

long long GenericCamera::getExposureTime(StrictLock<AbstractCamera> &camLock) {
	cout << "Dummy get exposure time of generic camera" << endl;
	cout << "Exposure get: " << stream0.get(CV_CAP_PROP_EXPOSURE) << endl;
	return long long();
}

bool GenericCamera::setExposureTime(StrictLock<AbstractCamera> &camLock, long long expo) {
	bool success = stream0.set(CV_CAP_PROP_EXPOSURE, expo);
	cout << "Dummy set exposure time of generic camera: " << success << endl;
	cout << "Exposure get: " << stream0.get(CV_CAP_PROP_EXPOSURE) << endl;
	return true;
}

bool GenericCamera::CalibrateDistortion(StrictLock<AbstractCamera> &camLock) {
	cout << "Dummy CalibrateDistortion of generic camera! " << endl;
	return true;
}