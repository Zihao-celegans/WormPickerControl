/*
	CameraFactory.cpp
	Alexander Kassouni
	Fang-Yen Lab
	Summer 2019

	A class for constructing and managing cameras
	Locks are generated whenever a new camera is made
	Cameras and locks are deleted when the factory leaves scope
*/

#include "CameraFactory.h"
#include "json.hpp"
#include "Config.h"
#include "AnthonysCalculations.h"

using namespace std;
using namespace cv;
using json = nlohmann::json;

CameraFactory* CameraFactory::instance = nullptr;

CameraFactory::CameraFactory(HardwareSelector& _use)
	:use(_use)
{
	baslerEncountered = false;
}

CameraFactory& CameraFactory::getInstance(HardwareSelector& use)
{
	if (!instance)
	{
		instance = new CameraFactory(use);
	}

	return *instance;
}

CameraFactory& CameraFactory::getInstance()
{
	return *instance;
}

bool CameraFactory::isInitialized()
{
	return instance;
}

CameraFactory::~CameraFactory() {
	deleteCameraLocks();
	deleteCameras();
	if (baslerEncountered) {
		Pylon::PylonTerminate();
	}

	//delete instance;
}


shared_ptr<AbstractCamera> CameraFactory::newCamera(AbstractCamera::camera_make make, int cam_num, 
	cv::Size acquire_size, cv::Size gray_size, cv::Size color_size, 
	bool flipHorizontal, bool flipVertical, long long exposure, int gain, double preferredMean, AbstractCamera::camera_running_mode run_mode, std::string f_additional_config) {
	
	shared_ptr<AbstractCamera> newCam;
	switch (make) {
	
	case AbstractCamera::BASLER:
	{
		if (!baslerEncountered) {
			baslerEncountered = true;
			Pylon::PylonInitialize();
		}
		newCam = shared_ptr<AbstractCamera> (new BaslerCamera(cam_num, acquire_size, gray_size, color_size, flipHorizontal, flipVertical, exposure, preferredMean));
		break;
	}	

	case AbstractCamera::GENERIC:
	{
		newCam = shared_ptr<AbstractCamera>(new GenericCamera(cam_num, acquire_size, gray_size, color_size, preferredMean, flipHorizontal, flipVertical, run_mode));
		break;
	}

	case AbstractCamera::FILE:
	{
		newCam = shared_ptr<AbstractCamera>(new GenericCamera(0, acquire_size, gray_size, color_size, preferredMean, flipHorizontal, flipVertical, run_mode, getFilesInPath(use.vidfilepath)[0]));
		break;
	}

	case AbstractCamera::THORLABS:
	{
		newCam = shared_ptr<AbstractCamera>(new ThorlabsCamera(cam_num, acquire_size, gray_size, color_size, exposure, gain, preferredMean, flipHorizontal, flipVertical, run_mode));
		break;
	}

	case AbstractCamera::IMAGINGSOURCE:
	{
		newCam = shared_ptr<AbstractCamera>(new ImagingSourceCamera(cam_num, acquire_size, gray_size, color_size, exposure, preferredMean, flipHorizontal, flipVertical, f_additional_config, run_mode));
		break;
	}

	default:
	{
		newCam = shared_ptr<AbstractCamera>(new GenericCamera(cam_num, acquire_size, gray_size, color_size, preferredMean, flipHorizontal, flipVertical, run_mode));
		break;
	}

	}
	locks[newCam] = shared_ptr<StrictLock<AbstractCamera>>(new StrictLock<AbstractCamera>(*newCam));
	cameras.push_back(shared_ptr<AbstractCamera>(newCam));
	return newCam;
}

void CameraFactory::loadAllCameras() {

	// ifstream cameras_json(cameras_json_fname);
	json cameras = Config::getInstance().getConfigTree("cameras");
	

	for (json camera : cameras) {
		// make each camera in the file

		// parse the make
		std::string make_str = camera["make"].get<string>();
		AbstractCamera::camera_make make;
		AbstractCamera::camera_running_mode run_mode;

		if (make_str == "FILE") {
			make = AbstractCamera::FILE;
			run_mode = use.cam ? AbstractCamera::READ_LOCAL: AbstractCamera::DUMMY;
		}
		else if (make_str == "BASLER") {
			make = AbstractCamera::BASLER;
			run_mode = use.cam ? AbstractCamera::LIVE : AbstractCamera::DUMMY;
		}
		else if (make_str == "GENERIC"){
			make = AbstractCamera::GENERIC;
			run_mode = use.cam ? AbstractCamera::LIVE : AbstractCamera::DUMMY;
		}
		else if (make_str == "THORLABS") {
			make = AbstractCamera::THORLABS;
			run_mode = use.cam ? AbstractCamera::LIVE : AbstractCamera::DUMMY;
		}
		else if (make_str == "IMAGINGSOURCE") {
			make = AbstractCamera::IMAGINGSOURCE;
			run_mode = use.cam ? AbstractCamera::LIVE : AbstractCamera::DUMMY;
		}
		else {
			cout << "Unrecognized camera make, using default";
			make = AbstractCamera::GENERIC;
			run_mode = AbstractCamera::DUMMY;
		}

		//read the camera num
		int cam_num = camera["cam num"].get<int>();
		
		// read the sizes
		Size acquire_size(camera["acquire resolution"]["width"].get<int>(), 
			camera["acquire resolution"]["height"].get<int>());
		Size color_size(camera["display resolution"]["width"].get<int>(),
			camera["display resolution"]["height"].get<int>());
		Size gray_size(camera["save resolution"]["width"].get<int>(),
			camera["save resolution"]["height"].get<int>());

		float preferredMean = camera["preferred mean"].get<float>();
		bool flipHorizontal = camera["flip horizontal"].get<bool>();
		bool flipVertical = camera["flip vertical"].get<bool>();
		//float exposure = camera["exposure"].get<float>();
		// Thorlabs and ImagingSource camera use long long
		long long exposure = camera["exposure"].get<long long>();
		int gain = camera["gain"].get<int>();

		// read the camera config files
		std::string f_config= camera["f_Config"].get<string>();

		newCamera(make, cam_num, acquire_size, gray_size, color_size, flipHorizontal, flipVertical, exposure, gain, preferredMean, run_mode, f_config);
	}
}

vector<int> CameraFactory::getCamNums() {
	vector<int> cam_nums;
	for (auto cam : cameras) {
		cam_nums.push_back(cam->getCamID());
	}
	return cam_nums;
}

vector<shared_ptr<AbstractCamera>> CameraFactory::getCameras() {
	return cameras;
}

shared_ptr<AbstractCamera> CameraFactory::firstCamera() {
	return cameras[0];
}

shared_ptr<AbstractCamera> CameraFactory::Nth_camera(int index) {
	return cameras[index];
}


int CameraFactory::numberCams() {
	return cameras.size();
}

shared_ptr<StrictLock<AbstractCamera>> CameraFactory::getCameraLock(shared_ptr<AbstractCamera> cam) {
	return locks[cam];
}


vector<shared_ptr<StrictLock<AbstractCamera>>> CameraFactory::getAllLocks() {
	vector<shared_ptr<StrictLock<AbstractCamera>>> allLocks;
	for (auto iter : locks) {
		allLocks.push_back(iter.second);
	}
	return allLocks;
}

void CameraFactory::deleteCameras() {
	//for (shared_ptr<AbstractCamera> iter : cameras) {
	//	delete iter;
	//}
	// Delete not necessary for shared_ptr
}

void CameraFactory::deleteCameraLocks() {
	//for (auto iter : locks) {
	//	delete iter.second;
	//}
	// Delete not necessary for shared_ptr
}