/*
	GenericCamera.h
	Alexander Kassouni
	Fang-Yen Lab
	Summer 2019

	A generic AbstractCamera implementation that uses opencv's VideoCapture
*/
#pragma once
#ifndef GENERICCAMERA_H
#define GENERICCAMERA_H

// Anthony's includes
#include "StrictLock.h"

// OpenCV includes
#include "opencv2\opencv.hpp"

// Boost includes
#include "boost\thread.hpp"
#include "boost\thread\lockable_adapter.hpp"

#include "AbstractCamera.h"

// Standard includes
#include <string>


class GenericCamera
	: public AbstractCamera
{

public:

	// Constructors
	GenericCamera(int n, cv::Size as, cv::Size gs, cv::Size cs, double pm, bool pfh, bool pfv, AbstractCamera::camera_running_mode run_mode, std::string videoFile = "");

	// Destructor - disconnects camera
	~GenericCamera();

	// Reconnect if connection fails
	bool reconnect(StrictLock<AbstractCamera> &camLock) override;

	// Acquire image
	bool grabImage(StrictLock<AbstractCamera> &camLock) override;

	cv::Mat grabFullRes(cv::Rect roi, StrictLock<AbstractCamera> &camLock) override;

	AbstractCamera::camera_make getMake() override;

	void getCamHandle(StrictLock<AbstractCamera> &camLock, void*& CamHandle) override;

	long long getExposureTime(StrictLock<AbstractCamera> &camLock) override;

	bool setExposureTime(StrictLock<AbstractCamera> &camLock, long long expo) override;

	bool CalibrateDistortion(StrictLock<AbstractCamera> &camLock) override;

protected:

	// Camera parameters
	cv::VideoCapture stream0;	/// camera or video file capture object

	// Alternative: Parameters for video file that is used for offline debugging
	std::string debug_file;

	// Images and related information
	std::string headline;			/// Text headline to display on image (not for samving)

	// Image adjustments
	int flip_horiz;				/// Flipping image horizontally. If not wanted, set to negative or zero values. 
	int flip_vert;				/// Flipping image vertically. If not wanted, set to negative or zero values. 


	// Camera running mode
	camera_running_mode cam_run_mode;

};

#endif