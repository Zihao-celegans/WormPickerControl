/*
	ThorlabsCamera.h
	Zihao Li
	Fang-Yen Lab
	Spring 2020

	An Thorlabs Camera implementation for interfacing with Thorlabs cameras
	that uses Thorlabs SDK and the opencv's VideoCapture
*/

#pragma once

#ifndef THORLABSCAMERA_H
#define THORLABSCAMERA_H

// Anthony's includes
#include "StrictLock.h"

// OpenCV includes
#include "opencv2\opencv.hpp"

// Boost includes
#include "boost\thread.hpp"
#include "boost\thread\lockable_adapter.hpp"

#include "AbstractCamera.h"

// Standard includes
#include <iostream>
#include <string>

class ThorlabsCamera : public AbstractCamera
{
public:
	// Constructors
	ThorlabsCamera(int n, cv::Size as, cv::Size gs, cv::Size cs, long long exposure, int gain, double pm, bool pfh, bool pfv, AbstractCamera::camera_running_mode run_mode);

	// Destructor - disconnects camera
	~ThorlabsCamera();

	// The callback that is registered with the function tl_camera_set_frame_available_callback
	void frame_available_callback(void* sender, unsigned short* image_buffer, int frame_count, unsigned char* metadata, int metadata_size_in_bytes, void* context);
	
	//Reports the given error string if it is not null and closes any opened resources. 
	//Returns the number of errors that occured during cleanup, +1 if error string was not null.
	int report_error_and_cleanup_resources(const char *error_string);

	// Initializes camera sdk and opens the first available camera. Returns a nonzero value to indicate failure.
	int initialize_camera_resources();

	// Reconnect if connection fails
	bool reconnect(StrictLock<AbstractCamera> &camLock) override;

	// Acquire image
	bool grabImage(StrictLock<AbstractCamera> &camLock) override;

	cv::Mat grabFullRes(cv::Rect roi, StrictLock<AbstractCamera> &camLock) override;

	AbstractCamera::camera_make getMake() override;

	// Return the camera handle
	void getCamHandle(StrictLock<AbstractCamera> &camLock, void*& CamHandle) override;

	long long getExposureTime(StrictLock<AbstractCamera> &camLock) override;

	bool setExposureTime(StrictLock<AbstractCamera> &camLock, long long expo) override;

	bool CalibrateDistortion(StrictLock<AbstractCamera> &camLock) override;

protected:

	// Camera running mode
	camera_running_mode cam_run_mode;

	// Alternative: Parameters for video file that is used for offline debugging
	std::string debug_file;

	// Images and related information
	std::string headline;			/// Text headline to display on image (not for samving)

	// Image adjustments
	int flip_horiz;				/// Flipping image horizontally. If not wanted, set to negative or zero values. 
	int flip_vert;				/// Flipping image vertically. If not wanted, set to negative or zero values. 

	// Variables in Thorlabs camera constructer
	int is_camera_sdk_open;
	int is_camera_dll_open;
	void *camera_handle;
	bool resources_initialzied;// Check whether resources are successfully initialized
	int binning_x; // Binnings that give the desired acquisition size
	int binning_y;
	long long EXPO;
	int GAIN;

	//int number_of_frame;

	//Frame variables in ThorlabsCamera::grabImage
	int height_pixels; //A reference that receives the value in pixels for the current image height.
	int width_pixels; //A reference that receives the value in pixels for the current image width.
	unsigned short *image_buffer;
	int frame_count;
	unsigned char *metadata;
	int metadata_size_in_bytes;
	bool valid_image_retrieved;// Check whether a valid image was grabbed
};

#endif
