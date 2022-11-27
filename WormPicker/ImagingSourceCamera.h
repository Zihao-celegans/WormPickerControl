/*
	ImagingSource.h
	Siming He & Zihao Li
	Fang-Yen Lab
	Summer 2022

	An ImagingSource Camera implementation that uses ImagingSource SDK
*/

#define _WIN32WINNT 0x0600

#ifndef IMAGINGSOURCECAMERA_H
#define IMAGINGSOURCECAMERA_H

#pragma once

// OpenCV includes
#include "opencv2\opencv.hpp"

// Boost includes
#include "boost\thread.hpp"
#include "boost\thread\lockable_adapter.hpp"

#include "AbstractCamera.h"

// Standard includes
#include <iostream>
#include <string>

// ImagingSource Camera SDK
#include <tisudshl.h>

class ImagingSourceCamera : public AbstractCamera
{
public:
	// Constructors
	ImagingSourceCamera(int n, cv::Size as, cv::Size gs, cv::Size cs, long long exposure, double pm, bool pfh, bool pfv, std::string config_file_name, AbstractCamera::camera_running_mode run_mode);

	// Destructor - clear buffer, stop camera stream
	~ImagingSourceCamera();

	
	// Load device from file
	bool setupDeviceFromFile(_DSHOWLIB_NAMESPACE::Grabber& gr,
		const std::string& devStateFilename);

	// Acquire image
	bool grabImage(StrictLock<AbstractCamera> &camLock) override;

	bool reconnect(StrictLock<AbstractCamera> &camLock) override;

	cv::Mat grabFullRes(cv::Rect roi, StrictLock<AbstractCamera> &camLock) override;

	AbstractCamera::camera_make getMake() override;

	// Return the camera handle
	void getCamHandle(StrictLock<AbstractCamera> &camLock, void*& CamHandle) override;

	long long getExposureTime(StrictLock<AbstractCamera> &camLock) override;

	bool setExposureTime(StrictLock<AbstractCamera> &camLock, long long expo_micro_sec) override;

	bool setExposureTimeSec(double expo_sec);

	bool setExposureTimeMicroSec(long long expo_micro_sec);

	bool CalibrateDistortion(StrictLock<AbstractCamera> &camLock) override;
	
protected:
	
	std::string f_config; // file name for config file

	DShowLib::Grabber grabber;

	DShowLib::tFrameSnapSinkPtr pSink;
	// Store the output type and dimension of the sink
	DShowLib::FrameTypeInfo info;
	// Declare a pointer to user buffer for the image
	// BYTE* pBuf[5];
	// Create a FrameQueueBuffer that wraps the memory of pBuf
	DShowLib::tFrameQueueBufferPtr ptr;

	DShowLib::tIVCDAbsoluteValuePropertyPtr exposure_absolute_ptr; // ptr for getting exposure absolute value


	// Image adjustments
	int flip_horiz;				/// Flipping image horizontally. If not wanted, set to negative or zero values. 
	int flip_vert;				/// Flipping image vertically. If not wanted, set to negative or zero values. 
	
	// Camera running mode
	camera_running_mode cam_run_mode;
};

#endif
