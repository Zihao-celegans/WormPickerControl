/*

	BaslerCam.cpp
	Alexander Kassouni
	Fang-Yen Lab
	Summer 2019

	An AbstractCamera implementation for interfacing with Basler cameras
	Uses the Pylon SDK

*/

#include "BaslerCam.h"
#include "AnthonysCalculations.h"

#include "ThreadFuncs.h"

using namespace Pylon;
using namespace GenApi;
using namespace cv;
using namespace std;


BaslerCamera::BaslerCamera(int cap_num, cv::Size acquire_size, cv::Size gray_size, cv::Size color_size, bool flipHorizontal, bool flipVertical, float exposure, float preferredMean)
	:AbstractCamera(cap_num, acquire_size, gray_size, color_size, preferredMean)
{

	CTlFactory& tlFactory = CTlFactory::GetInstance();

	DeviceInfoList_t devices;

	// check if there is a camera connected to the computer
	if (tlFactory.EnumerateDevices(devices, true) == 0) {
		// setup as non basler cam
		throw NoBaslerCameraException();
	}

	int dev_idx = 0;

	device = tlFactory.CreateDevice(devices[dev_idx]);
	cam.Attach(device);
	bool temp = cam.IsPylonDeviceAttached();
	string n = cam.GetDeviceInfo().GetFullName();
	boost_sleep_ms(1000);
	cam.Open();

	CAcquireContinuousConfiguration().OnAttached(cam);
	cam.StartGrabbing(GrabStrategy_LatestImageOnly);

	formatConverter.OutputPixelFormat = PixelType_Mono8;
	cout << cam.GetDeviceInfo().GetModelName() << " camera initialized" << endl;
	cout << "port:" << cam.GetDeviceInfo().GetPortID() << endl;

	//cam.ExposureTime = exposure;
	//cam.ReverseX = flipVertical;
	//cam.ReverseY = flipHorizontal;
	
}

BaslerCamera::~BaslerCamera() {
	cam.StopGrabbing();
	cam.Close();
}

bool BaslerCamera::reconnect(StrictLock<AbstractCamera> &camLock) {
	// todo
	return false;
}

bool BaslerCamera::grabImage(StrictLock<AbstractCamera> &camLock) {
	
	time_stamp = getCurrentTimeString(false, true);

	cam.RetrieveResult(1000, grabPtr);
	formatConverter.Convert(pylonImage, grabPtr);
	if (!grabPtr->GrabSucceeded()) {
		log_message = "ERROR: No image returned from cam" + to_string(cap_num) + " at " + time_stamp;
		return false;
	}
	
	Mat cvImage = Mat(grabPtr->GetHeight(), grabPtr->GetWidth(), CV_8UC1, (uint8_t *)pylonImage.GetBuffer());
	
	if (!validateImage(cvImage)) {
		log_message = "ERROR: No image returned from cam" + to_string(cap_num) + " at " + time_stamp;
		return false;
	}
	
	resize(cvImage, img_gray, gray_size);
	log_message = "Image acquired at " + time_stamp + " on cam" + to_string(cap_num);
	return true;

}

cv::Mat BaslerCamera::grabFullRes(cv::Rect roi, StrictLock<AbstractCamera> &camLock) {
	return Mat();
}

AbstractCamera::camera_make BaslerCamera::getMake() {
	return AbstractCamera::BASLER;
}

void BaslerCamera::getCamHandle(StrictLock<AbstractCamera> &camLock, void*& CamHandle) {
	cout << "Dummy get Basler camera handle" << endl;
	return void();
}

long long BaslerCamera::getExposureTime(StrictLock<AbstractCamera> &camLock) {
	cout << "Dummy get exposure time of Basler camera" << endl;
	return long long();
}

bool BaslerCamera::setExposureTime(StrictLock<AbstractCamera> &camLock, long long expo) {
	cout << "Dummy set exposure time of Basler camera" << endl;
	return true;
}

bool BaslerCamera::CalibrateDistortion(StrictLock<AbstractCamera> &camLock) {
	cout << "Dummy CalibrateDistortion of Basler camera! " << endl;
	return true;
}