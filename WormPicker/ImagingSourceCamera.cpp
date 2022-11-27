/*
	ThorlabsCamera.cpp
	Siming He & Zihao Li
	Fang-Yen Lab
	Summer 2022

	An ImagingSource Camera implementation that uses ImagingSource SDK
*/

#include "ImagingSourceCamera.h"

// Anthony's includes
#include "stdafx.h"
#include "AnthonysCalculations.h"
#include "ImageFuncs.h"
#include "MatIndex.h"
#include "SharedOutFile.h"
#include "ThreadFuncs.h"
#include "ErrorNotifier.h"
#include <boost/filesystem.hpp>

// OpenCV includes
#include "opencv2\opencv.hpp"


// Boost includes
#include "boost\thread.hpp"


// Namespaces
using namespace std;
using namespace cv;
using namespace std::chrono;
using namespace DShowLib;


// FrameTypeInfo info;
// BYTE* pBuf;
// tFrameQueueBufferPtr ptr;

// Constructor
ImagingSourceCamera::ImagingSourceCamera(int n, Size as, Size gs, Size cs, long long exposure, double pm, bool pfh, bool pfv, std::string config_file_name, AbstractCamera::camera_running_mode run_mode)
	: AbstractCamera(n, as, gs, cs, pm)
{

	// Initialize camera running mode
	cam_run_mode = run_mode;

	// get live frame from camera
	if (cam_run_mode == DUMMY) {
		cout << "Grab frame from ImagingSource camera in DUMMY mode! " << endl;
	}
	else if (cam_run_mode == LIVE) {

		// initialize the Imaging Source library
		DShowLib::InitLibrary();

		f_config = config_file_name;

		// Setup the device from file
		if (!setupDeviceFromFile(grabber, f_config))
		{
			std::cerr << "Imaging Source Camera Device Setup File NOT FOUND" << endl;
		}

		grabber.setOverlayBitmapPathPosition(ePP_NONE);

		// Setup sink with image buffer format eY800
		pSink = FrameSnapSink::create(eY800);

		// Set the sink.
		grabber.setSinkType(pSink);

		// Prepare the live mode, to get the output size of the sink.
		if (!grabber.prepareLive(false))
		{
			std::cerr << "Could not render the VideoFormat into a eY800 sink.";
		}
		else {
			grabber.startLive(false);
			std::cerr << "Connected to camera %d\n" << cap_num << endl;
		}

		// Retrieve the output type and dimension of the sink
		pSink->getOutputFrameType(info);

		// Set the exposure time
		setExposureTimeMicroSec(exposure);
	
	}

	// Setup remaining parameters
	flip_horiz = pfh;
	flip_vert = pfv;

	// If no resizing (preferred size is negative), then set the acquire size as preferred size
	if (color_size.width < 0 || color_size.height < 0) color_size = acquire_size;

	if (gray_size.width < 0 || gray_size.height < 0) gray_size = acquire_size;

	// Establish empty images
	img_color = Mat(color_size.height, color_size.width, CV_8UC3, Scalar(0, 0, 0));
	img_gray = Mat(gray_size.height, gray_size.width, CV_8U, Scalar(0, 0, 0));

}

// Destructor - clear buffer, stop camera stream
ImagingSourceCamera::~ImagingSourceCamera() {
	// Stop the live video.
	grabber.stopLive();

	// Close the device.
	grabber.closeDev();
}

bool ImagingSourceCamera::reconnect(StrictLock<AbstractCamera> &camLock) {

	// Wait at least 5 seconds
	boost_sleep_ms(5000);

	// Stop the live video.
	grabber.stopLive();

	// Close the device.
	grabber.closeDev();

	boost_sleep_ms(500);


	// Restart 
	// initialize the Imaging Source library
	DShowLib::InitLibrary();
	// Setup the device from file
	if (!setupDeviceFromFile(grabber, f_config))
	{
		std::cerr << "Imaging Source Camera Device Setup File NOT FOUND" << endl;
		return false;
	}

	grabber.setOverlayBitmapPathPosition(ePP_NONE);

	// Setup sink with image buffer format eY800
	pSink = FrameSnapSink::create(eY800);

	// Set the sink.
	grabber.setSinkType(pSink);

	// Prepare the live mode, to get the output size of the sink.
	if (!grabber.prepareLive(false))
	{
		std::cerr << "Could not render the VideoFormat into a eY800 sink.";
		return false;
	}
	else {
		grabber.startLive(false);
		std::cerr << "Connected to camera %d\n\n" << cap_num << endl;

		// Retrieve the output type and dimension of the sink
		pSink->getOutputFrameType(info);

		return true;
	}

}

// Acquire image
bool ImagingSourceCamera::grabImage(StrictLock<AbstractCamera> &camLock) {

	// Matrix to store the acquired data
	Mat img_acq;

	bool valid_image_retrieved = false;

	// get dummy (empty) frame
	if (cam_run_mode == DUMMY) {
		img_acq = Mat::zeros(acquire_size.height, acquire_size.width, CV_8U);

		valid_image_retrieved = true;
	}

	// get live frame from camera
	else if (cam_run_mode == LIVE) {
		const int length = 1; // Grab single image once a time
		BYTE* pBuf[length] = {};
		tFrameQueueBufferList lst;

		// create empty buffers
		for (int i = 0; i < length; ++i)
		{
			pBuf[i] = new BYTE[info.buffersize];
			DShowLib::Error err = createFrameQueueBuffer(ptr, info, pBuf[i], info.buffersize, NULL);
			if (err.isError()) {
				std::cerr << "Failed to create buffer due to " << err.toString() << "\n";
				return false;
			}
			lst.push_back(ptr);
		}

		// fill the buffers with snapshots
		DShowLib::Error err = pSink->snapSequence(lst, lst.size());

		if (err.isError()) {
			std::cerr << "Failed to snap into buffers due to " << err.toString() << "\n";
			return false;
		}

		// Read the data from the buffers to opencv Mat (WormPicker variables)
		img_acq = Mat::zeros(1, info.buffersize, CV_8U);
		memcpy(img_acq.data, pBuf[0], info.buffersize);
		img_acq = img_acq.reshape(1, info.dim.cy);

		// Delete the buffer
		lst.clear();
		for (int j = 0; j < length; ++j)
		{
			delete pBuf[j];
		}

		// Check whether a valid image was grabbed
		valid_image_retrieved = validateImage(img_acq);
	}


	if (valid_image_retrieved) {

		// Adjust image multiplicitive gain
		if (preferredMean > 0) {
			double mulGain = preferredMean / mean(img_acq)[0];
			img_acq = img_acq * mulGain;
		}

		// Flip horizontal axis (always on for offline mode to prevent mirror images)
		if (flip_horiz)
			flip(img_acq, img_acq, 1);

		if (flip_vert)
			flip(img_acq, img_acq, 0);

		// Hand image over for use and convert to grayscale
		img_acq.copyTo(img_gray);

		cvtColor(img_gray, img_color, COLOR_GRAY2RGB);

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


cv::Mat ImagingSourceCamera::grabFullRes(cv::Rect roi, StrictLock<AbstractCamera> &camLock) {
	return Mat();
}

AbstractCamera::camera_make ImagingSourceCamera::getMake() {
	return AbstractCamera::IMAGINGSOURCE;
}

// Return the camera handle
void ImagingSourceCamera::getCamHandle(StrictLock<AbstractCamera> &camLock, void*& CamHandle) {
}

long long ImagingSourceCamera::getExposureTime(StrictLock<AbstractCamera> &camLock) {

	// Retrieve the property item ptr
	exposure_absolute_ptr = grabber.getVCDPropertyInterface<IVCDAbsoluteValueProperty>(VCDID_Exposure, VCDElement_Value);

	if (exposure_absolute_ptr == NULL)
	{
		std::cout << "Exposure property has no absolute value interface" << std::endl;
		return -1;
	}
	else
	{
		std::cout << "Current Exposure absolute value of camera " << to_string(cap_num)  << ": " << std::fixed << exposure_absolute_ptr->getValue() << " " << exposure_absolute_ptr->getDimType() << std::endl;
		//std::cout << "Exposure absolute range: [" << exposure_absolute_ptr->getRangeMin() << ".." << exposure_absolute_ptr->getRangeMax() << "]" << std::endl;

		return exposure_absolute_ptr->getValue();
	}

}


bool ImagingSourceCamera::setExposureTimeSec(double expo_sec) {

	// Retrieve the property item ptr
	exposure_absolute_ptr = grabber.getVCDPropertyInterface<IVCDAbsoluteValueProperty>(VCDID_Exposure, VCDElement_Value);

	if (exposure_absolute_ptr->getAvailable()) {
		if (expo_sec >= exposure_absolute_ptr->getRangeMin() && expo_sec <= exposure_absolute_ptr->getRangeMax()) {
			exposure_absolute_ptr->setValue(expo_sec);
			cout << "Exposure of camera " << to_string(cap_num) << " set to : " << to_string(expo_sec) << " " << exposure_absolute_ptr->getDimType() << endl;
			return true;
		}
		else {
			cout << "Failed to set exposure of camera " << to_string(cap_num) << " to: " << to_string(expo_sec) << endl;
			std::cout << "Valid exposure absolute range: [" << exposure_absolute_ptr->getRangeMin()
				<< ".." << exposure_absolute_ptr->getRangeMax() << "]" << std::endl;
			return false;
		}
	}
	else {
		cout << "Failed to set exposure of camera " << to_string(cap_num) << " to: " << to_string(expo_sec) << endl;
		cout << "Absolute value interface for exposure UNAVAILABLE!" << endl;
	}

}


bool ImagingSourceCamera::setExposureTime(StrictLock<AbstractCamera> &camLock, long long expo_micro_sec) {
	return setExposureTimeMicroSec(expo_micro_sec);
}


bool ImagingSourceCamera::setExposureTimeMicroSec(long long expo_micro_sec) {

	double expo_sec = double(expo_micro_sec) / double(1000000);

	return setExposureTimeSec(expo_sec);
}

// Load device from file
bool ImagingSourceCamera::setupDeviceFromFile(_DSHOWLIB_NAMESPACE::Grabber& gr,
	const std::string& devStateFilename)
{
	
	if (gr.loadDeviceStateFromFile(devStateFilename))
	{
		std::cout << "Device opened from: <" << devStateFilename << ">." << std::endl;
	}
	else
	{
		std::cout << "File <" << devStateFilename << "> either not found, or device could not be opened from it." << std::endl;
	}
	/*
	if (!gr.showDevicePage() || !gr.isDevValid())
	{
		return false;
	}
	gr.saveDeviceStateToFile(devStateFilename, true, true, false);
	*/
	return true;
}

// Calibrate the field distortion in the image
// Code adapted from:
// https://learnopencv.com/camera-calibration-using-opencv/
bool ImagingSourceCamera::CalibrateDistortion(StrictLock<AbstractCamera> &camLock) {

	// Defining the dimensions of checkerboard
	int CHECKERBOARD[2]{ 6,9 };

	// Directory of calibration pattern
	std::string calib_path = "C:/Users/Yuchi/Dropbox/UPenn_2022_Fall/WormPicker/Experiment_data/20221020_Cam_Calibration/Pattern_images/*.jpg";

	// Directory of test images
	std::string test_path = "C:/Users/Yuchi/Dropbox/UPenn_2022_Fall/WormPicker/Experiment_data/20221020_Cam_Calibration/Test_images/*.jpg";

	// Directory for saving recognized patterns
	std::string save_pattern_path = "C:/Users/Yuchi/Dropbox/UPenn_2022_Fall/WormPicker/Experiment_data/20221020_Cam_Calibration/Recognized_pattern/";

	// Directory for saving undistorted images
	std::string save_path = "C:/Users/Yuchi/Dropbox/UPenn_2022_Fall/WormPicker/Experiment_data/20221020_Cam_Calibration/Undistorted_images/";

	// Creating vector to store vectors of 3D points for each checkerboard image
	std::vector<std::vector<cv::Point3f> > objpoints;

	// Creating vector to store vectors of 2D points for each checkerboard image
	std::vector<std::vector<cv::Point2f> > imgpoints;

	// Defining the world coordinates for 3D points
	std::vector<cv::Point3f> objp;
	for (int i{ 0 }; i < CHECKERBOARD[1]; i++)
	{
		for (int j{ 0 }; j < CHECKERBOARD[0]; j++)
			objp.push_back(cv::Point3f(j, i, 0));
	}

	// Extracting path of individual image stored in a given directory
	std::vector<cv::String> calib_imgs;
	// Path of the folder containing checkerboard images
	cv::glob(calib_path, calib_imgs);

	cv::Mat frame, gray;
	// vector to store the pixel coordinates of detected checker board corners 
	std::vector<cv::Point2f> corner_pts;
	bool success;

	// Looping over all the images in the directory
	for (int i{ 0 }; i < calib_imgs.size(); i++)
	{

		std::cout << "Image " << i << " : " << calib_imgs[i] << std::endl;

		frame = cv::imread(calib_imgs[i]);
		cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

		// Finding checker board corners
		// If desired number of corners are found in the image then success = true  
		success = cv::findChessboardCorners(gray, cv::Size(CHECKERBOARD[0], CHECKERBOARD[1]), corner_pts, CV_CALIB_CB_ADAPTIVE_THRESH | CV_CALIB_CB_FAST_CHECK | CV_CALIB_CB_NORMALIZE_IMAGE);

		/*
			* If desired number of corner are detected,
			* we refine the pixel coordinates and display
			* them on the images of checker board
		*/
		if (success)
		{

			std::cout << "Image " << i << " was found checkerboard pattern! " << std::endl;

			cv::TermCriteria criteria(CV_TERMCRIT_EPS | CV_TERMCRIT_ITER, 30, 0.001);

			// refining pixel coordinates for given 2d points.
			cv::cornerSubPix(gray, corner_pts, cv::Size(11, 11), cv::Size(-1, -1), criteria);

			// Displaying the detected corner points on the checker board
			cv::drawChessboardCorners(frame, cv::Size(CHECKERBOARD[0], CHECKERBOARD[1]), corner_pts, success);

			objpoints.push_back(objp);
			imgpoints.push_back(corner_pts);


		}

		std::string pattern_img_p = calib_imgs[i].operator std::string();
		boost::filesystem::path pat_img_p(pattern_img_p);

		// Save recognized patterns
		imwrite(save_pattern_path + pat_img_p.stem().string() + "_labelled.jpg", frame);

		// resize frame for display
		Mat disp_frame;
		resize(frame, disp_frame, Size(), 0.5, 0.5);
		cv::imshow("Image", disp_frame);
		cv::waitKey(500);
	}

	cv::destroyAllWindows();

	cv::Mat cameraMatrix, distCoeffs, R, T;

	/*
		* Performing camera calibration by
		* passing the value of known 3D points (objpoints)
		* and corresponding pixel coordinates of the
		* detected corners (imgpoints)
	*/
	cv::calibrateCamera(objpoints, imgpoints, cv::Size(gray.rows, gray.cols), cameraMatrix, distCoeffs, R, T);

	std::cout << "cameraMatrix : " << cameraMatrix << std::endl;
	std::cout << "distCoeffs : " << distCoeffs << std::endl;
	std::cout << "Rotation vector : " << R << std::endl;
	std::cout << "Translation vector : " << T << std::endl;


	// Undistort images
	cv::Size imageSize = frame.size();
	Mat map1, map2, orig_img, undist_img;

	// Get the undistortion maps
	cv::initUndistortRectifyMap(
		cameraMatrix, distCoeffs, Mat(),
		getOptimalNewCameraMatrix(cameraMatrix, distCoeffs, imageSize, 1, imageSize, 0), imageSize,
		CV_16SC2, map1, map2);


	// Extracting path of individual image stored in the test directory
	std::vector<cv::String> test_imgs;
	// Path of the folder containing test images
	cv::glob(test_path, test_imgs);

	for (int i{ 0 }; i < test_imgs.size(); i++)
	{
		// Read test images
		orig_img = imread(test_imgs[i], IMREAD_COLOR);
		if (orig_img.empty())
			continue;

		// Undistort
		AnthonysTimer Ta;
		remap(orig_img, undist_img, map1, map2, INTER_LINEAR);
		cout << "Time for remap one frame: " << Ta.getElapsedTime() << "s" << endl;;

		std::string test_img_p = test_imgs[i].operator std::string();
		boost::filesystem::path p(test_img_p);

		cout << p.filename().string() << endl;

		// Save undistorted images to folder
		imwrite(save_path + p.stem().string() + "_undist.jpg", undist_img);

		// resize image for display
		Mat disp_undist_img;
		resize(undist_img, disp_undist_img, Size(), 0.5, 0.5);
		cv::imshow("Undistorted Image", disp_undist_img);
		cv::waitKey(500);
	}

	

	return true;

}