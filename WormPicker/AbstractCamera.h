/*
	AbstractCamera.h
	Alexander Kassouni
	Fang-Yen Lab
	Summer 2019

	An abstract class which defines an interface for new cameras
	GenericCamera and BaslerCamera inherit from this class

	AbstractCamera
		  |
		  |-----------------------|------------------ ...
		  |						  |
	GenericCamera			BaslerCamera

	To add functionality for a new camera, create a class that inherits from
	AbstractCamera and implements the virtual functions. grabImage() should write
	the image to img_gray and img_color. Add the constructor to CameraFactory.cpp
*/

#pragma once

#include "opencv2/opencv.hpp"

#include "boost\thread.hpp"
#include "boost\thread\lockable_adapter.hpp"

#include "StrictLock.h"
#include "ErrorNotifier.h"
#include "SharedOutFile.h"
#include "MatIndex.h"



class AbstractCamera
	:public boost::basic_lockable_adapter<boost::mutex>
{
public:

	AbstractCamera::AbstractCamera(int cap_num, cv::Size acquire_size, cv::Size gray_size, cv::Size color_size, float preferredMean);

	AbstractCamera::~AbstractCamera();
	
	enum camera_make { GENERIC, BASLER, FILE, THORLABS, IMAGINGSOURCE };

	enum camera_running_mode {LIVE, READ_LOCAL, DUMMY}; // LIVE: get live frame; READ_LOCAL: get frame from local file; DUMMY: get dummy (empty) frame

	// PURE VIRTUAL FUNCTIONS - MUST BE OVERIDDEN IN SUBCLASSES

	virtual bool reconnect(StrictLock<AbstractCamera> &camLock) = 0;	//reconnect to the camera

	virtual bool grabImage(StrictLock<AbstractCamera> &camLock) = 0;	// populate img_gray and img_color with the image

	virtual cv::Mat grabFullRes(cv::Rect roi, StrictLock<AbstractCamera> &camLock) = 0;

	virtual void getCamHandle(StrictLock<AbstractCamera> &camLock, void*& CamHandle) = 0; // get the camera handle;

	virtual long long getExposureTime(StrictLock<AbstractCamera> &camLock) = 0; // Get the exposure time of camera

	virtual bool setExposureTime(StrictLock<AbstractCamera> &camLock, long long expo) = 0; // Set the exposure time of camera

	virtual bool CalibrateDistortion(StrictLock<AbstractCamera> &camLock) = 0; // Calibrate field distortion of camera

	virtual camera_make getMake() = 0;	//get the make of the camera

	// NON-VIRTUAL FUNCTIONS
	static bool validateImage(cv::Mat img_test);

	//void grabImageThread(StrictLock<AbstractCamera> &camLock, ErrorNotifier &Err);

	cv::Mat getCurrentImgGray(StrictLock<AbstractCamera> &camLock);

	cv::Mat getCurrentImgColor(StrictLock<AbstractCamera> &camLock);

	std::string getTimeStamp(StrictLock<AbstractCamera> &camLock) const;

	bool updateTimeStamp(StrictLock<AbstractCamera> &camLock);

	std::string getCurrentLogMessage(StrictLock<AbstractCamera> &camLock) const;

	void setLogMessage(StrictLock<AbstractCamera> &camLock, std::string msg);

	cv::Size getColorSize() const;

	cv::Size getGraySize() const;

	int getCamID() const;

	bool getValidImageState() const;

	void displayCamOptions() { throw "not implemented"; }

	void saveDistortInfo() {};
protected:
	int cap_num;
	std::string time_stamp, log_message;
	cv::Mat img_gray, img_color;
	cv::Size acquire_size, gray_size, color_size;
	bool valid_image_retrieved;
	float preferredMean;
};

void grabImageThread(std::shared_ptr<AbstractCamera> cam, std::shared_ptr<StrictLock<AbstractCamera>> camLock, ErrorNotifier &Err, double &fps);
void grabImageThreadNoFps(std::shared_ptr<AbstractCamera> cam, std::shared_ptr<StrictLock<AbstractCamera>> camLock, ErrorNotifier &Err); // Non FPS-tracking version (e.g. Picker)
