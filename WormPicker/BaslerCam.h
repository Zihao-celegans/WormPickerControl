/*

	BaslerCam.h
	Alexander Kassouni
	Fang-Yen Lab
	Summer 2019

	An AbstractCamera implementation for interfacing with Basler cameras
	Uses the Pylon SDK

*/

#pragma once

// Boost includes
#include "boost\thread.hpp"
#include "boost\thread\lockable_adapter.hpp"

#include "opencv2/opencv.hpp"

//pylon includes
#include "pylon/PylonIncludes.h"
#include "pylon/BaslerUniversalInstantCamera.h"

#include "StrictLock.h"
#include "AbstractCamera.h"


class NoBaslerCameraException : public std::exception {
	virtual const char* what() const throw() {
		return "No basler camera found";
	}
};


class BaslerCamera
	: public AbstractCamera
{

public:
	BaslerCamera(int cap_num, cv::Size acquire_size, cv::Size gray_size, cv::Size color_size, bool flipHorizontal, bool flipVertical, float exposure, float preferredMean);
	~BaslerCamera();
	bool reconnect(StrictLock<AbstractCamera> &camLock) override;	//
	bool grabImage(StrictLock<AbstractCamera> &camLock) override;	// read an image and write it to the img_gray and img_color mats
	cv::Mat grabFullRes(cv::Rect roi, StrictLock<AbstractCamera> &camLock) override;
	AbstractCamera::camera_make getMake() override;
	void getCamHandle(StrictLock<AbstractCamera> &camLock, void*& CamHandle) override;
	long long getExposureTime(StrictLock<AbstractCamera> &camLock) override;
	bool setExposureTime(StrictLock<AbstractCamera> &camLock, long long expo) override;
	bool CalibrateDistortion(StrictLock<AbstractCamera> &camLock) override;

private:
	Pylon::IPylonDevice* device;
	Pylon::CGrabResultPtr grabPtr;
	Pylon::CBaslerUniversalInstantCamera cam;
	Pylon::CImageFormatConverter formatConverter;
	Pylon::CPylonImage pylonImage;
	
};