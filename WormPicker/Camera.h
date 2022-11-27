/*
	Camera.h
	Anthony Fouad
	Fang-Yen Lab
	Fall 2018

	Stores information about the camera and contains methods for requesting and timestamping images.
	Some default values for the camera are specified
*/
#pragma once
#ifndef CAMERA_H
#define CAMERA_H

// Anthony's includes
#include "StrictLock.h"
#include "MatIndex.h"
#include "SharedOutFile.h"
#include "QRcode.h"
#include "ErrorNotifier.h"
#include "HardwareSelector.h"

// OpenCV includes
#include "opencv2\opencv.hpp"

// Boost includes
#include "boost\thread.hpp"
#include "boost\thread\lockable_adapter.hpp"


// Standard includes
#include <vector>
#include <string>

class Camera 
	: public boost::basic_lockable_adapter<boost::mutex> 
{

public:

	// Constructors
	// if model name is empty or not listed, the first camera found will be used
	Camera(int n, cv::Size as, cv::Size cs, cv::Size gs, double pm, int pr, int pfh, int pfv, HardwareSelector &_use);
	Camera(std::string fname_camera_params, HardwareSelector &_use);

	//Camera(const Camera& cam0) = default;
	void setupCamera(int n, cv::Size as, cv::Size cs, cv::Size gs, double pm, int pr, int pfh, int pfv);

	// Destructor - disconnects camera
	~Camera();

	// Reconnect if connection fails
	bool reconnect(StrictLock<Camera> &camLock);

	// Find all system cameras
	int findSystemCameras(StrictLock<Camera> &camLock);

	// Acquire image
	bool grabImage(bool image_requested, StrictLock<Camera> &camLock);

	// Validate image (not empty or blank)
	bool validateImage(cv::Mat img_test, StrictLock<Camera> &camLock);

	// Search for and read a bar code within the image
	bool scanForQrCode(StrictLock<Camera> &camLock);

	// Display gui for camera control
	void displayCamOptions(StrictLock<Camera> &camLock);

	// get a high resolution region of interest
	//cv::Mat getROI();

	// Getter
	std::string getTimeStamp(StrictLock<Camera> &camLock) const;
	std::string getCurrentLogMessage(StrictLock<Camera> &camLock) const;
	std::vector<double> getTimeVals(StrictLock<Camera> &camLock) const;
	cv::Mat &getCurrImgColor(StrictLock<Camera> &camLock); /// Reference (not copy) to modify data in object without duplicating
	cv::Mat &getCurrImgGray(StrictLock<Camera> &camLock);
	cv::Size getColorSize() const;
	cv::Size getGraySize() const;
	bool getValidImageState() const;		/// true if last image was validly retrieved
	int getId() const;
	MatIndex getQrRowCol() const;
	
protected:

	// Camera parameters
	std::string fname_camera_params;		/// Full file name of the cam parameters
	int cap_num;				/// Camera number on system, usually 0
	cv::VideoCapture stream0;	/// camera or video file capture object
	
	cv::Size acquire_size;		/// Acquisition width and height
	int exposure;				/// WARNING: OpenCV exposure setting may not be compatible with the IC cameras

	// Alternative: Parameters for video file that is used for offline debugging
	std::string debug_file;

	// Camera or file state
	bool is_open;
	bool valid_image_retrieved; /// False if camera gets disconnected
	std::string log_message; /// Message to be added to the log

	// Images and related information
	cv::Mat img_color, img_gray;
	std::string headline;			/// Text headline to display on image (not for samving)
	std::string time_stamp;			/// Text of the image time stamp
	std::vector <double> time_vals;	/// Numeric coefficients of the time stamp
	QRcode Qr;

	// Image adjustments
	double preferred_mean;		/// Multiplicitive gain. If not wanted, set to a negative value
	cv::Size color_size;		/// Resize color images. If not wanted, set W and H to negative values (these are displayed)
	cv::Size gray_size;			/// Resize gray images (the ones saved to disk)
	int rotate_image;			/// Rotate acquired images. If not wanted, set to a negative value. 
								///							If wanted, set to an OPENCV code such as ROTATE_90_CLOCKWISE
	int flip_horiz;				/// Flipping image horizontally. If not wanted, set to negative or zero values. 
	int flip_vert;				/// Flipping image vertically. If not wanted, set to negative or zero values. 
	HardwareSelector &use;		/// Whether to use camera or video file
};




/*
	NON-MEMBER HELPER FUNCTIONS AND THREAD FUNCTIONS
*/



// Create cameras and locks
void loadAllCameras(std::string fname_camera_params, std::vector<std::shared_ptr<Camera>>& all_cameras, HardwareSelector &use);
void createAllCamLocks(const std::vector<std::shared_ptr<Camera>>& cameras, std::vector<std::shared_ptr<StrictLock<Camera>>>&);

// Thread function for grabbing an image continuously or when requested
void grabImageThreadFunc(std::shared_ptr<Camera> cam0, std::shared_ptr<StrictLock<Camera>> camLock, bool *request_image,
	ErrorNotifier &Err, double& fps, std::string& latest_timestamp);

// Thread function for saving an image when requested
void saveImageThreadFunc(std::shared_ptr<cv::Mat>& img_save, std::shared_ptr<std::string> full_save_name,
	std::shared_ptr<std::string> full_save_path, std::string* current_LJ_error,
	std::shared_ptr<std::string> current_cam_error, SharedOutFile &current_log, StrictLock<SharedOutFile> &current_log_lock,
	std::string *plogs, MatIndex &bar_loc);


#endif