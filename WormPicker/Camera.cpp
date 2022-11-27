/*
	Camera.cpp
	Anthony Fouad
	Fang-Yen Lab
	Fall 2018

	Stores information about the camera and contains methods for requesting and timestamping images
*/

// Anthony's includes
#include "stdafx.h"
#include "Camera.h"
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

// Constructor: parameters specified in text file
Camera::Camera(string fcp, HardwareSelector &_use) : use(_use) {

	// Setup text input
	fname_camera_params = fcp;
	ifstream inputfile(fname_camera_params);

	// Declare temporary variables
	int n = 0, pr = 0, pfh = 0, pfv = 0;
	Size as(0, 0);
	Size cs(0, 0);
	Size gs(0, 0); 
	double pm = 0;

	// Read data from file
	inputfile >> n;
	inputfile >> as.width;		
	inputfile >> as.height;
	inputfile >> cs.width;
	inputfile >> cs.height;
	inputfile >> gs.width;
	inputfile >> gs.height;
	inputfile >> pm;
	inputfile >> pr;
	inputfile >> pfh;
	inputfile >> pfv;

	// Send params to the setup function
	setupCamera(n, as, cs, gs, pm, pr, pfh, pfv);
}


// Constructor: parameters specified as arguments
Camera::Camera(int n, cv::Size as, cv::Size cs, cv::Size gs, double pm, int pr, int pfh, int pfv, HardwareSelector &_use) :
	use(_use)
{
	setupCamera(n, as, cs, gs, pm, pr, pfh, pfv);
}

// Copy constructor


void Camera::setupCamera(int n, Size as, Size cs, Size gs, double pm, int pr, int pfh, int pfv) {

	// Initialize camera with requested parameters
	acquire_size = as;
	if (use.cam) {
		cap_num = n;
		stream0 = VideoCapture(cap_num + cv::CAP_DSHOW);
		stream0.set(CAP_PROP_FRAME_HEIGHT, acquire_size.height);
		stream0.set(CAP_PROP_FRAME_WIDTH, acquire_size.width);
	}

	// Alternative: Setup file name for debugging video
	else {
		debug_file = getFilesInPath(use.vidfilepath)[0];
		stream0 = VideoCapture(debug_file);
	}

	// verify that camera or file was opened
	boost_sleep_ms(250);
	is_open = stream0.isOpened();
	if (!is_open && !use.cam)
		printf("Attempted to use file but failed to open file: %s\n\n", debug_file.c_str());

	else if (!is_open && use.cam)
		printf("Attempted to use camera but failed to open camera %d\n\n",cap_num);

	else if (is_open && !use.cam)
		printf("Opened file: %s\n\n", debug_file.c_str());

	else if (is_open && use.cam)
		printf("Connected to camera %d\n\n", cap_num);


	// Setup remaining parameters
	preferred_mean = pm;
	color_size = cs;
	gray_size = gs;
	rotate_image = pr;
	flip_horiz = pfh;
	flip_vert = pfv;

	// If no resizing (preferred size is negative), then set the acquire size as preferred size
	if (color_size.width < 0 || color_size.height < 0)
		color_size = acquire_size;

	if (gray_size.width < 0 || gray_size.height < 0)
		gray_size = acquire_size;

	// Seed the time vals list
	for (int i = 0; i < 7; i++)
		time_vals.push_back(0);

	// Establish empty images
	img_color = Mat(color_size.height, color_size.width, CV_8UC3, Scalar(0, 0, 0));
	img_gray = Mat(gray_size.height, gray_size.width, CV_8U, Scalar(0, 0, 0));

}

// Destructor - disconnects cameras automatically when stream0 object destructs
Camera::~Camera() {
	stream0.release();
}

// Reconnect (e.g. if connection fails)
bool Camera::reconnect(StrictLock<Camera> &camLock) {

	// Wait at least 5 seconds before attempting to reopen in order to avoid the popup window prompt for camera selection
	boost_sleep_ms(5000);

	// Attempt to reconnect to the camera
	stream0.release();
	boost_sleep_ms(500);
	stream0.open(cap_num);
	stream0.set(CAP_PROP_FRAME_HEIGHT, acquire_size.height);
	stream0.set(CAP_PROP_FRAME_WIDTH, acquire_size.width);
	boost_sleep_ms(500);

	// If camera is opened, 
	is_open = stream0.isOpened();

	// Return camera open state
	return is_open;

}



// Acquire image
bool Camera::grabImage(bool image_requested, StrictLock<Camera> &camLock) {

	// Attempt to grab an image
	Mat img_acq;
	log_message.clear();
	stream0.read(img_acq);

	// If no image was grabbed because we are using a video file and reached the end, go back to the beginning, 
	if (img_acq.empty() && !use.cam) {
		stream0.set(CAP_PROP_POS_MSEC, 0);
		stream0.read(img_acq);
	}

		// Wait before validating
		// boost_sleep_ms(250);

	// Check whether a valid image was grabbed
	valid_image_retrieved = validateImage(img_acq, camLock);

	// get the image timestamp
	time_stamp = getCurrentTimeString(time_vals, false, true); 

	// Perform preprocessing and hand over image for use ONLY if image is requested 
	/// (as opposed to being used for buffer flushing)
	if (image_requested && valid_image_retrieved) {

		//resize(img_acq, img_acq, Size(640,480));

		// Adjust image multiplicitive gain
		if (preferred_mean > 0) {
			double mulGain = preferred_mean / mean(img_acq)[0];
			img_acq = img_acq * mulGain;
		}

		// Rotate the image
		if (rotate_image>=-90)
			rotate(img_acq, img_acq, rotate_image);

		// Flip horizontal axis (always on for offline mode to prevent mirror images)
		if (flip_horiz)
			flip(img_acq, img_acq, 1);

		if (flip_vert)
			flip(img_acq, img_acq, 0);

		// Hand image over for use and convert to grayscale
		img_acq.copyTo(img_color);
		img_acq.copyTo(img_gray);
		cvtColor(img_color, img_gray, COLOR_RGB2GRAY);

		// Resize images if specified 
		if (color_size.width > 0 &&
			color_size.height > 0 &&
			color_size.width != acquire_size.width &&
			color_size.height != acquire_size.height)
			resize(img_color, img_color, color_size);

		if (gray_size.width > 0 &&
			gray_size.height > 0 &&
			gray_size.width != acquire_size.width &&
			gray_size.height != acquire_size.height)
			resize(img_gray, img_gray, gray_size);


		// Update log
		log_message = "Image acquired at " + time_stamp + " on cam" + to_string(cap_num);
	}

	// If image was requested but is not valid (e.g. disconnect) set a dummy zero image with text label
	else if (image_requested && !valid_image_retrieved) {
		
		// Set both to blank zeros (they probably already are already blank)
		img_color = 0;
		img_gray = 0;

		// Add disconnect message
		putText(img_color, string("Invalid image - is camera connected?"), Point(10, (int)(img_color.rows / 2)), 1, 1, colors::yy, 1);
		putText(img_gray, string("Invalid image - is camera connected?"), Point(10, (int)(img_gray.rows / 2)), 1, 1, colors::ww, 1);

		// Set an error message in the log
		log_message = "ERROR: No image returned from cam" + to_string(cap_num) + " at " + time_stamp ;
	}

	// Return the validity state for external use
	return valid_image_retrieved;
}

// Validate image
bool Camera::validateImage(Mat img_test, StrictLock<Camera> &camLock) {

	// Detemine whether this image is blank
	bool is_blank = false;
	Scalar mean_val, stdev_val;
	meanStdDev(img_test, mean_val, stdev_val);
	is_blank = stdev_val[0] == 0;

	// However, fully saturated due to blue light (255) should not count as 
	is_blank = is_blank && mean_val[0] < 254;

	// Retrun true if image is neither blank nor empty
	return !img_test.empty() && !is_blank;

}

// Search for and read bar code
bool Camera::scanForQrCode(StrictLock<Camera> &camLock) {

	// Scan for the bar code and attempt to read it
	return false; // Qr.scanAndDecode(img_gray);
}

// Display camera options
void Camera::displayCamOptions(StrictLock<Camera> &camLock) {
	stream0.set(CAP_PROP_SETTINGS, 1);
}

// Find system cameras
int Camera::findSystemCameras(StrictLock<Camera> &camLock) {

	int camid = 0;
	
	for (int i = camid; i<3; i++) {
		VideoCapture stream_test = VideoCapture(i);
		Mat test_img;
		stream_test.read(test_img);
		cout << "Valid Cameras:\t";
		if (validateImage(test_img, camLock)) {
			if (camid == -2) {
				camid = i;
				cout << camid << " ";
				break;
			}
		}
		stream_test.release();
	}
	return camid;
}

// Getters
string Camera::getTimeStamp(StrictLock<Camera> &camLock) const { return time_stamp; }
string Camera::getCurrentLogMessage(StrictLock<Camera> &camLock) const { return log_message; }
vector<double> Camera::getTimeVals(StrictLock<Camera> &camLock) const { return time_vals; }
Mat &Camera::getCurrImgColor(StrictLock<Camera> &camLock) { return img_color; }
Mat &Camera::getCurrImgGray(StrictLock<Camera> &camLock) { return img_gray; }
Size Camera::getColorSize() const { return color_size; }
Size Camera::getGraySize() const { return gray_size; }
bool Camera::getValidImageState() const { return valid_image_retrieved; }
int Camera::getId() const { return cap_num; }
MatIndex Camera::getQrRowCol() const { return Qr.curr_qr_loc; }





// Non-member helper functions






// Create a vector of camera objects corresponding to each profile.
void loadAllCameras(string fname_camera_params, vector<shared_ptr<Camera>>& all_cameras, HardwareSelector &use) {

	// Setup text input
	ifstream inputfile(fname_camera_params);

	// Declare temporary variables
	int n = 0, pr = 0, pfh = 0, pfv = 0;
	Size as(0, 0);
	Size cs(0, 0);
	Size gs(0, 0);
	double pm = 0;

	// Setup vector

	// Create all camera objects. Each object should a list of data with the same format right after each other

	while (inputfile >> n &&
		inputfile >> as.width &&
		inputfile >> as.height &&
		inputfile >> cs.width &&
		inputfile >> cs.height &&
		inputfile >> gs.width &&
		inputfile >> gs.height &&
		inputfile >> pm &&
		inputfile >> pr &&
		inputfile >> pfh &&
		inputfile >> pfv)
	{
		all_cameras.push_back(shared_ptr<Camera>(new Camera(n, as, cs, gs, pm, pr, pfh, pfv, use)));
	}

	inputfile.close();
}

// Create a camera lock corresponding to each camera object

void createAllCamLocks(const vector<shared_ptr<Camera>>& cameras, vector<shared_ptr<StrictLock<Camera>>>& camLocks) {

	for (int i = 0; i < cameras.size(); i++) {
		camLocks.push_back(shared_ptr<StrictLock<Camera>>(new StrictLock<Camera>(*cameras[i])));
	}

}


// Thread function for acquiring an image continuously. 
///		* When image is actually requested, the acquired image is handed over to cam0->img_color and 
///			img_gray and the timestamp values are updated. 
///		* See Camera->grabImage for more information.
///		* When image is not requested, it will still be acquired (e.g. for buffer flushing) but
///			the img_ variables, timestamp, and preprocessing steps will not be updated / run.

void grabImageThreadFunc(shared_ptr<Camera> cam0, shared_ptr<StrictLock<Camera>> camLock,
	bool *request_image, ErrorNotifier &Err, double& fps, string& timestamp) {

	AnthonysTimer Tframe;

	for (;;) {

		// Check for exit request
		Tframe.setToZero();
		camLock->unlock();
		boost_sleep_ms(25); //25
		boost_interrupt_point();

		// Lock the camera object for the remainder of this iteration
		camLock->lock();

		// Grab image. Reconnect continuously if failed. Camera always locked when grabbing an image.
		int reconnect_tries = 0;
		while (!cam0->grabImage(*request_image, *camLock)) {

			/// Check for exit and increment tries
			boost_interrupt_point();
			reconnect_tries++;

			/// Unlock camera during reconnect sequence
			camLock->unlock();

			/// Reconnect
			Err.notify(errors::camera_reconnect_attempt);
			cam0->reconnect(*camLock);

			/// Alert the user by email if we've been disconnected for a while
			if (reconnect_tries == 10) {
				Err.notify(errors::camera_disconnect);
			}

			/// Lock again before grabbing new image
			camLock->lock();
		}

		// Get info about current frame
		fps = 1/Tframe.getElapsedTime();
		timestamp = cam0->getTimeStamp(*camLock);
	}
}