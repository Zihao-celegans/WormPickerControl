/*
	ThorlabsCamera.cpp
	Zihao Li
	Fang-Yen Lab
	Spring 2020

	A Thorlabs Camera implementation for interfacing with Thorlabs cameras
	that uses Thorlabs SDK and the opencv's built-in library
*/

#include "ThorlabsCamera.h"

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
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include "windows.h"
#include <chrono> // precise clock to count the execution time

// Boost includes
#include "boost\thread.hpp"

// Thorlabs Camera SDK
#include "tl_camera_sdk.h"
#include "tl_camera_sdk_load.h"
#include "tl_camera_sdk_load.c"



// Namespaces
using namespace std;
using namespace cv;
using namespace std::chrono;


// Constructor: parameters specified as arguments
// when file is the empty string, load a camera
// otherwise, use the file name to load a recording



ThorlabsCamera::ThorlabsCamera(int n, cv::Size as, cv::Size gs, cv::Size cs, long long exposure, int gain, double pm, bool pfh, bool pfv, AbstractCamera::camera_running_mode run_mode)
	: AbstractCamera(n, as, gs, cs, pm)
{

	// Initialize construtor variables
	is_camera_sdk_open = 0;
	is_camera_dll_open = 0;
	camera_handle = 0;
	binning_x = 1;
	binning_y = 1;
	EXPO = exposure;
	GAIN = gain;
	
	//initialize frame variables
	height_pixels = acquire_size.height;
	width_pixels = acquire_size.width;
	image_buffer = 0;
	frame_count = 0;
	metadata = 0;
	metadata_size_in_bytes = 0;

	boost_sleep_ms(250);

	// Initialize camera running mode
	cam_run_mode = run_mode;

	// get live frame from camera
	if (cam_run_mode == DUMMY) {
		cout << "Grab frame from Thorlabs camera in DUMMY mode! " << endl;
	}
	else if (cam_run_mode == LIVE) {
		// Check whether the camera is connected & resources is initialized or not
		if (initialize_camera_resources()) {
			// Check whether resources are successfully initialized: fail:0; success:1
			resources_initialzied = 0;
			printf("Attempted to use camera but failed to open Thorlabs camera %d\n\n", cap_num);
			// To do: end the program or reboot if fails to initialize camera resources
		}
		else {
			resources_initialzied = 1;
			printf("Connected to Thorlabs camera %d\n\n", cap_num);
		}

		if (resources_initialzied == 1)
		{
			// Set the exposure time
			if (tl_camera_set_exposure_time(camera_handle, exposure)) {
				report_error_and_cleanup_resources(tl_camera_get_last_error());
			}
			else { printf("Camera exposure set to %lld microsec.\n", exposure); }

			// Set the gain
			if (tl_camera_set_gain(camera_handle, gain)) {
				report_error_and_cleanup_resources(tl_camera_get_last_error());
			}
			else { printf("Camera gain set to %d\n", gain); }

			//Set the binning according to the desired aquistion size
			//Different from other cameras, Thorlabs camera does not support acquistion size directly
			//set by the users to the camera. The only way to change acquistion size by the users is to
			//tune the binning.
			binning_x = 2448 / (acquire_size.width);
			binning_y = 2048 / (acquire_size.height);
			// To do: generate an error messsage if binning are not whole numbers,
			// i.e. 2448 is not divisible by acquire_size.width

			// Set the x binning
			if (tl_camera_set_binx(camera_handle, binning_x)) {
				report_error_and_cleanup_resources(tl_camera_get_last_error());
			}
			else { printf("Camera x binning set to %d\n", binning_x); }

			// Set the y binning
			if (tl_camera_set_biny(camera_handle, binning_y)) {
				report_error_and_cleanup_resources(tl_camera_get_last_error());
			}
			else { printf("Camera y binning set to %d\n", binning_y); }

			// Configure camera for continuous acquisition by setting the number of frames to 0.
			if (tl_camera_set_frames_per_trigger_zero_for_unlimited(camera_handle, 0)) {
				report_error_and_cleanup_resources(tl_camera_get_last_error());
			}

			// To do: to verify whether this piece of code is needed in the frame-available-mode acquisition
			// Set camera to wait 100 ms for a frame to arrive during a poll.
			// If an image is not received in 100ms, the returned frame will be null
			if (tl_camera_set_image_poll_timeout(camera_handle, 100)) {
				report_error_and_cleanup_resources(tl_camera_get_last_error());
			}

			// Arm the camera, preparing it for imaging
			if (tl_camera_arm(camera_handle, 2)) {
				report_error_and_cleanup_resources(tl_camera_get_last_error());
			}
			else { printf("Camera armed\n"); }

			// Once the camera is initialized and armed, this function sends a SOFTWARE trigger command to the camera over USB
			if (tl_camera_issue_software_trigger(camera_handle)) {
				report_error_and_cleanup_resources(tl_camera_get_last_error());
			}
			else { printf("Software trigger sent\n"); }

		}
		else {
			printf("Failed to prepare Thorlabs camera %d for acquiring images because camera resources have NOT been initialized.\n\n", cap_num);
		}
	
	}

	// Setup remaining parameters
	flip_horiz = pfh;
	flip_vert = pfv;

	// If no resizing (preferred size is negative), then set the acquire size as preferred size
	if (color_size.width < 0 || color_size.height < 0) color_size = acquire_size;
	if (gray_size.width < 0 || gray_size.height < 0) gray_size = acquire_size;

	// Establish empty images
	img_color = Mat(color_size.height, color_size.width, CV_8UC3, Scalar(0, 0, 0));
	img_gray = Mat(gray_size.height, gray_size.width, CV_16U, Scalar(0, 0, 0));
}

// Destructor - disconnects cameras
ThorlabsCamera::~ThorlabsCamera()
{
	// Stop the camera.
	if (tl_camera_disarm(camera_handle))
		printf("Failed to stop the Thorlabs camera %d\n\n", cap_num);

	// Clean up and exit
	report_error_and_cleanup_resources((char*)"0");
}

void ThorlabsCamera::frame_available_callback(void* sender, unsigned short* image_buffer, int frame_count, unsigned char* metadata, int metadata_size_in_bytes, void* context)
{
	cout << "frame_available_callback" << endl;
	/*printf("image buffer = 0x%p\n", image_buffer);
	printf("frame_count = %d\n", frame_count);
	printf("meta data buffer = 0x%p\n", metadata);
	printf("metadata size in bytes = %d\n", metadata_size_in_bytes);*/

	//SetEvent(frame_acquired_event);
	// If you need to save the image data for application specific purposes, this would be the place to copy it into separate buffer.
}


int ThorlabsCamera::report_error_and_cleanup_resources(const char *error_string)
{
	int num_errors = 0;
	if (error_string)
	{
		printf("Error: %s\n", error_string);
		num_errors++;
	}

	printf("Closing all resources...\n");

	if (camera_handle)
	{
		if (tl_camera_close_camera(camera_handle))
		{
			printf("Failed to close camera!\n%s\n", tl_camera_get_last_error());
			num_errors++;
		}
		camera_handle = 0;
	}
	if (is_camera_sdk_open)
	{
		if (tl_camera_close_sdk())
		{
			printf("Failed to close SDK!\n");
			num_errors++;
		}
		is_camera_sdk_open = 0;
	}
	if (is_camera_dll_open)
	{
		if (tl_camera_sdk_dll_terminate())
		{
			printf("Failed to close dll!\n");
			num_errors++;
		}
		is_camera_dll_open = 0;
	}

	printf("Closing resources finished.\n");
	return num_errors;
}

int ThorlabsCamera::initialize_camera_resources()
{

	// Initializes camera dll
	if (tl_camera_sdk_dll_initialize())
		return report_error_and_cleanup_resources((char*) "Failed to initialize dll!\n");
	printf("Successfully initialized dll\n");
	is_camera_dll_open = 1;

	// Open the camera SDK
	if (tl_camera_open_sdk())
		return report_error_and_cleanup_resources((char*) "Failed to open SDK!\n");
	printf("Successfully opened SDK\n");
	is_camera_sdk_open = 1;

	char camera_ids[1024];
	camera_ids[0] = 0;

	// Discover cameras.
	if (tl_camera_discover_available_cameras(camera_ids, 1024))
		return report_error_and_cleanup_resources(tl_camera_get_last_error());
	printf("camera IDs: %s\n", camera_ids);

	// Check for no cameras.
	if (!strlen(camera_ids))
		return report_error_and_cleanup_resources((char*) "Did not find any cameras!\n");

	// Camera IDs are separated by spaces.
	char* p_space = strchr(camera_ids, ' ');
	if (p_space)
		*p_space = '\0'; // isolate the first detected camera
	char first_camera[256];

	// Copy the ID of the first camera to separate buffer (for clarity)
	strcpy_s(first_camera, 256, camera_ids);
	printf("First camera_id = %s\n", first_camera);


	// Connect to the camera (get a handle to it).
	if (tl_camera_open_camera(first_camera, &camera_handle))
		return report_error_and_cleanup_resources(tl_camera_get_last_error());
	printf("Camera handle = 0x%p\n", camera_handle);

	return 0;
}

bool ThorlabsCamera::reconnect(StrictLock<AbstractCamera> &camLock)
{
	resources_initialzied = 0;
	bool isReconnect = false; // Keep the value as false until the camera is reconnected
	// Wait at least 5 seconds before attempting to reopen in order to avoid the popup window prompt for camera selection
	boost_sleep_ms(5000);

	// Attempt to reconnect to the camera
	boost_sleep_ms(500);
	// Close the camera first
	if (camera_handle != 0) {
		if (tl_camera_disarm(camera_handle))
		{
			printf("Failed to stop and reconnect the Thorlabs camera %d\n\n", cap_num);
			//return isReconnect;
		}
		report_error_and_cleanup_resources((char*)"0");
	}


	// Reinitialize the camera resources
	if (initialize_camera_resources()) {
		resources_initialzied = 0;
		printf("Attempted to use camera but failed to reopen Thorlabs camera %d\n\n", cap_num);
		return isReconnect;
	}
	else {
		resources_initialzied = 1;
		printf("Reconnected to Thorlabs camera %d\n\n", cap_num);
		binning_x = 2448 / (acquire_size.width);
		binning_y = 2048 / (acquire_size.height);
		boost_sleep_ms(500);

		if (tl_camera_set_exposure_time(camera_handle, EXPO)) {
			report_error_and_cleanup_resources(tl_camera_get_last_error());
		}
		else { printf("Camera exposure set to %lld microsec.\n", EXPO); }

		if (tl_camera_set_gain(camera_handle, GAIN)) {
			report_error_and_cleanup_resources(tl_camera_get_last_error());
		}
		else { printf("Camera gain set to %d\n", GAIN); }

		// Set the x binning
		if (tl_camera_set_binx(camera_handle, binning_x)) {
			report_error_and_cleanup_resources(tl_camera_get_last_error());
		}
		else { printf("Camera x binning set to %d\n", binning_x); }

		// Set the y binning
		if (tl_camera_set_biny(camera_handle, binning_y)) {
			report_error_and_cleanup_resources(tl_camera_get_last_error());
		}
		else { printf("Camera y binning set to %d\n", binning_y); }

		if (tl_camera_set_frames_per_trigger_zero_for_unlimited(camera_handle, 0)) {
			report_error_and_cleanup_resources(tl_camera_get_last_error());
		}

		if (tl_camera_set_image_poll_timeout(camera_handle, 100)) {
			report_error_and_cleanup_resources(tl_camera_get_last_error());
		}

		// Arm the camera, preparing it for imaging
		if (tl_camera_arm(camera_handle, 2)) {
			report_error_and_cleanup_resources(tl_camera_get_last_error());
		}
		else { printf("Camera armed\n"); }

		if (tl_camera_issue_software_trigger(camera_handle)) {
			report_error_and_cleanup_resources(tl_camera_get_last_error());
		}
		else { printf("Software trigger sent\n"); }
		isReconnect = TRUE;
		return isReconnect;
	}

	//Reset the binning to get the desired acquisition size

	// To do: Return the camera open state
}

bool ThorlabsCamera::grabImage(StrictLock<AbstractCamera> &camLock)
{

	// Matrix to store the acquired data
	Mat img_acq;

	bool valid_image_retrieved = false;

	// get dummy (empty) fram
	if (cam_run_mode == DUMMY) {
		img_acq = Mat::zeros(acquire_size.height, acquire_size.width, CV_8U);

		valid_image_retrieved = true;
	}	
	
	// get live frame from camera
	else if (cam_run_mode == LIVE) {
		// Start to grab images only when camera resources are successfully initialized i.e. resources_initialzied = 1

		auto start = high_resolution_clock::now();
		for (;;)
		{
			auto duration = duration_cast<microseconds>(high_resolution_clock::now() - start);
			if (duration.count() > 1000000)
				break;
			if (resources_initialzied != 0) {
				if (tl_camera_get_pending_frame_or_null(camera_handle, &image_buffer, &frame_count, &metadata, &metadata_size_in_bytes)) {
					report_error_and_cleanup_resources(tl_camera_get_last_error());
					break;
					//return valid_image_retrieved;
				}
			}

			if (image_buffer)
				break;
		}
		//if (!image_buffer) {
		//	continue; //timeout
		//	}
		if (image_buffer != 0) {
			try {
				if (tl_camera_get_image_height(camera_handle, &height_pixels)) {
					report_error_and_cleanup_resources(tl_camera_get_last_error());
					//return valid_image_retrieved;
				}
				if (tl_camera_get_image_width(camera_handle, &width_pixels)) {
					report_error_and_cleanup_resources(tl_camera_get_last_error());
					//return valid_image_retrieved;
				}
			}
			catch (...) { cout << "default exception" << endl; }
		}

		Mat img_acq_16 = Mat::zeros(1, height_pixels*width_pixels, CV_16U);
		Mat Nor_imgacq;

		double min, max;

		if (image_buffer != 0)
		{
			memcpy(img_acq_16.data, image_buffer, 1 * height_pixels*width_pixels * sizeof(ushort));
			threshold(img_acq_16, img_acq_16, 4095, 4095, THRESH_TRUNC);

			img_acq_16.convertTo(img_acq, CV_8UC1, 255.0 / 4095.0, 0);

			minMaxLoc(img_acq, &min, &max);

			if (false) {
				normalize(img_acq, img_acq, 0, 255, NORM_MINMAX);
			}
			//Nor_imgacq.convertTo(img_acq, CV_8UC1);

			img_acq = img_acq.reshape(1, height_pixels);
		}
		else
		{
			img_acq = Mat::zeros(height_pixels, width_pixels, CV_8UC1);
		}


		// Check whether a valid image was grabbed
		valid_image_retrieved = validateImage(img_acq);

		if (image_buffer) { valid_image_retrieved = true; }
		image_buffer = 0;

	}

	
	// Perform preprocessing and hand over image for use ONLY if image is requested 
	/// (as opposed to being used for buffer flushing)

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

		// Hand image over for use and convert to grayscale To do: check whether need to convert
		img_acq.copyTo(img_color);
		img_acq.copyTo(img_gray);
		cvtColor(img_gray, img_color, COLOR_GRAY2RGB);

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

	return valid_image_retrieved;

}

cv::Mat ThorlabsCamera::grabFullRes(cv::Rect roi, StrictLock<AbstractCamera> &camLock) {
	return Mat();
}

AbstractCamera::camera_make ThorlabsCamera::getMake() {
	return AbstractCamera::THORLABS;
}

void ThorlabsCamera::getCamHandle(StrictLock<AbstractCamera> &camLock, void*& CamHandle) {
	CamHandle = camera_handle;
	return void();
}

long long ThorlabsCamera::getExposureTime(StrictLock<AbstractCamera> &camLock) {

	long long expo = 0;
	bool fail = tl_camera_get_exposure_time(camera_handle, &expo);

	if (fail) {
		report_error_and_cleanup_resources(tl_camera_get_last_error());
	}
	else { printf("Camera current exposure: %lld microsec.\n", expo); }

	return expo;
}

bool ThorlabsCamera::setExposureTime(StrictLock<AbstractCamera> &camLock, long long expo) {

	bool fail = tl_camera_set_exposure_time(camera_handle, expo);

	if (fail) {
		report_error_and_cleanup_resources(tl_camera_get_last_error());
	}
	else { printf("Camera exposure set to %lld microsec.\n", expo); }

	return !fail;
}


bool ThorlabsCamera::CalibrateDistortion(StrictLock<AbstractCamera> &camLock) {
	cout << "Dummy CalibrateDistortion of Thorlabs camera! " << endl;
	return true;
}
