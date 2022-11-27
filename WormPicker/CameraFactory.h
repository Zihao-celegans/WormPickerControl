/*
	CameraFactory.h
	Alexander Kassouni
	Fang-Yen Lab
	Summer 2019

	A class for constructing and managing cameras
	Locks are generated whenever a new camera is made
	Cameras and locks are deleted when the factory leaves scope by RAII
*/

#pragma once

#include "HardwareSelector.h"

#include "AbstractCamera.h"
#include "BaslerCam.h"
#include "GenericCamera.h"
#include "ThorlabsCamera.h"
#include "ImagingSourceCamera.h"


#include "pylon/PylonIncludes.h"
#include <vector>

#include "opencv2/opencv.hpp"


// todo - make singleton
class CameraFactory
{
public:
	
	static CameraFactory& getInstance(HardwareSelector& use);

	//can only be called when you know that instance is nonnull (so you don't have to pass in use every time)
	static CameraFactory& getInstance();

	//responsibility is on the caller to make sure it isn't null
	static bool isInitialized();

	~CameraFactory();
	
	// make a new camera based on the make
	std::shared_ptr<AbstractCamera> newCamera(AbstractCamera::camera_make make, int cam_num,
		cv::Size acquire_size, cv::Size gray_size, cv::Size color_size,
		bool flipHorizontal, bool flipVertical, long long exposure, int gain, double preferredMean, AbstractCamera::camera_running_mode run_mode, std::string f_additional_config);

	void loadAllCameras();

	int numberCams();

	std::vector<std::shared_ptr<AbstractCamera>> getCameras();

	std::vector<int> getCamNums();	// not garunteed to be uniqie?

	std::shared_ptr<AbstractCamera> firstCamera();

	std::shared_ptr<AbstractCamera> Nth_camera(int index); // Starts from 0. 0 means the first camera, 1 for the second cam etc.

	std::shared_ptr<StrictLock<AbstractCamera>> getCameraLock(std::shared_ptr<AbstractCamera> cam);

	std::vector<std::shared_ptr<StrictLock<AbstractCamera>>> getAllLocks();
	
	//delete all initialized cameras
	//calls each objects respective destructor
	void deleteCameras();

	void deleteCameraLocks();
private:
	CameraFactory(HardwareSelector& use);

	static CameraFactory* instance;

	std::vector<std::shared_ptr<AbstractCamera>> cameras;	//a vector containing pointers to every camera made by the factory
	std::map< std::shared_ptr<AbstractCamera>, std::shared_ptr<StrictLock<AbstractCamera>>> locks;	// map of camera ptrs to lock ptrs
	bool baslerEncountered;
	const HardwareSelector& use;
};

