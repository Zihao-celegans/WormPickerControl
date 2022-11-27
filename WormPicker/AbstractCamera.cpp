/*
	AbstractCamera.cpp
	Alexander Kassouni
	Fang-Yen Lab
	Summer 2019

	An abstract class which defines an interface for new cameras
	GenericCamera and BaslerCamera inherit from this class

	See header for more info

*/
#pragma once
#include "AbstractCamera.h"

#include "ThreadFuncs.h"
#include "AnthonysCalculations.h"
#include "AnthonysTimer.h"

#include <string>

using namespace std;

AbstractCamera::AbstractCamera(int cap_num, cv::Size acquire_size, cv::Size gray_size, cv::Size color_size, float preferredMean) 
	:cap_num(cap_num), acquire_size(acquire_size), gray_size(gray_size), color_size(color_size), preferredMean(preferredMean)
{
	return;
}

AbstractCamera::~AbstractCamera() {
	return;
}

bool AbstractCamera::validateImage(cv::Mat img_test) {

	// If the image is empty return immediately
	if (img_test.empty()) {
		return false;
	}

	// Detemine whether this image is blank
	bool is_blank = false;
	cv::Scalar mean_val, stdev_val;
	cv::meanStdDev(img_test, mean_val, stdev_val);
	is_blank = stdev_val[0] == 0;

	// However, fully saturated due to blue light (255) should not count as 
	is_blank = is_blank && mean_val[0] < 254;

	// Retrun true if image is neither blank nor empty
	return !img_test.empty() && !is_blank;
}

/*
void AbstractCamera::grabImageThread(StrictLock<AbstractCamera> &camLock, ErrorNotifier &Err) {
	while (true) {

		// Check for exit request
		camLock.unlock();
		boost_sleep_ms(25); //25
		boost_interrupt_point();

		// Lock the camera object for the remainder of this iteration
		camLock.lock();

		// get the timestamp
		time_stamp = getCurrentTimeString(false, true);

		if (grabImage(camLock)) {
			log_message = "Image acquired at " + time_stamp + " on cam" + to_string(cap_num);
			validImage = true;
			imshow("img", img_gray);
			cv::waitKey(1);
		}
		else {
			log_message = "ERROR: No image returned from cam" + to_string(cap_num) + " at " + time_stamp;
			img_color = 0;
			img_gray = 0;
			validImage = false;
		}
	}
}*/

cv::Mat AbstractCamera::getCurrentImgGray(StrictLock<AbstractCamera> &camLock) {
	return img_gray;
}

cv::Mat AbstractCamera::getCurrentImgColor(StrictLock<AbstractCamera> &camLock) {
	return img_color;
}

bool AbstractCamera::updateTimeStamp(StrictLock<AbstractCamera> &camLock) {
	time_stamp = getCurrentTimeString(false, true);
	return true;
}

std::string AbstractCamera::getTimeStamp(StrictLock<AbstractCamera> &camLock) const {
	return time_stamp;
}

std::string AbstractCamera::getCurrentLogMessage(StrictLock<AbstractCamera> &camLock) const {
	return log_message;
}

void AbstractCamera::setLogMessage(StrictLock<AbstractCamera> &camLock, std::string msg) {
	log_message = msg;
	return;
}

cv::Size AbstractCamera::getColorSize() const{
	return color_size;
}

cv::Size AbstractCamera::getGraySize() const {
	return gray_size;
}

int AbstractCamera::getCamID() const {
	return cap_num;
}

bool AbstractCamera::getValidImageState() const {
	return valid_image_retrieved;
}

void saveImageThreadFunc(shared_ptr<cv::Mat>& img_save,
	shared_ptr<string> full_save_name,
	shared_ptr<string> full_save_path,
	string *current_LJ_error,
	shared_ptr<string> current_cam_error,
	SharedOutFile &current_log,
	StrictLock<SharedOutFile> &current_log_lock,
	string *plogs,
	MatIndex &bar_loc) {

	string fname_current_log = *plogs + "log_";

	for (;;) {

		// Check for exit request
		boost_sleep_ms(25);
		boost_interrupt_point();

		// Take no action if save name is not specified
		if (full_save_name->size() == 0)
			continue;

		// Verify that the output path exists
		makePath(*full_save_path);

		// Otherwise save the image, locking is accomplished by the WWExperiment object and is not needed here
		imwrite(*full_save_name, *img_save);


		// Verify that a log file for today's date is open. If not, terminate it and open a new one. File is appended to, not overwritten
		current_log_lock.lock();
		string todays_log = *plogs + "log_" + getCurrentDateString() + ".txt";
		if (todays_log.compare(fname_current_log) != 0) {
			current_log.stream.close();
			fname_current_log = todays_log;
			makePath(*plogs);
			current_log.stream = ofstream(fname_current_log, std::ofstream::app);

			/// Add label headers
			current_log.stream << "<Current_time>\t" <<
				"<Full_save_name>\t" <<
				"<Barcode row>\t" <<
				"<Barcode col>\t" <<
				"<I_mean>\t" <<
				"<I_stdev>\t" <<
				"<LabJack>\t" <<
				"<Cam>" << endl;
		}

		// Append to the log
		cv::Scalar mean_val(0, 0, 0, 0), stdev_val(0, 0, 0, 0);
		meanStdDev(*img_save, mean_val, stdev_val);
		current_log.stream << getCurrentTimeString(true) << "\t"
			<< *full_save_name << "\t"
			<< bar_loc.row << "\t"
			<< bar_loc.col << "\t"
			<< "Mean=" << mean_val[0] << "\t"
			<< "Std=" << stdev_val[0] << "\t"
			<< "LJ=" << *current_LJ_error << "\t"
			<< "CAM=" << *current_cam_error << endl;
		current_log_lock.unlock();

		// Indicate that saving is finished by clearing full_save_name
		full_save_name->clear();
	}
}


// TODO, make static member function?
void grabImageThread(shared_ptr<AbstractCamera> cam, shared_ptr<StrictLock<AbstractCamera>> camLock, ErrorNotifier &Err, double& fps) {
	//int loopnum = 0;
	while (true) {
		//cout << "Cam looped " << loopnum << " , " << fps << endl;
		//loopnum = loopnum + 1;
		// Check for exit request
		camLock->unlock();
		//boost_sleep_ms(25); //25
		boost_interrupt_point();

		// Lock the camera object for the remainder of this iteration
		//AnthonysTimer tTest;
		camLock->lock();

		int reconnectTries = 0;
		AnthonysTimer Ta;
		while (cam->updateTimeStamp(*camLock) && !cam->grabImage(*camLock)) {
			/// Check for exit and increment tries
			boost_interrupt_point();
			reconnectTries++;

			/// Unlock camera during reconnect sequence
			camLock->unlock();

			/// Reconnect
			Err.notify(errors::camera_reconnect_attempt);
			cam->reconnect(*camLock);

			/// Alert the user by email if we've been disconnected for a while
			if (reconnectTries == 10) {
				Err.notify(errors::camera_disconnect);
			}

			/// Lock again before grabbing new image
			camLock->lock();
		}
		fps = 1 / Ta.getElapsedTime();
		//tTest.printElapsedTime("", true);
		//if (cam->getCamID() == 0){ cout << "fps of " << cam->getCamID() << " = " << fps << endl; }
		//cout << "fps of " << cam->getCamID() << " = " << fps << endl;
	}
}

// Non-FPS tracking version (e.g. wormpicker)
void grabImageThreadNoFps(std::shared_ptr<AbstractCamera> cam, std::shared_ptr<StrictLock<AbstractCamera>> camLock, ErrorNotifier &Err) {

	double fps = 0; /// Trash FPS, not used in this version
	grabImageThread(cam, camLock, Err, fps);

}
