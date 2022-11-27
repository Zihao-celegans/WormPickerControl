/*
	LidHandler.h
	Peter Bowlin
	August 2021


	This class handles everything that has to do with managing lids for the robot
*/

#pragma once
#include "PlateTray.h"
#include "OxCnc.h"
#include "MatIndex.h"

#include <vector>

// OpenCV includes
#include "opencv2\opencv.hpp"

struct LidGrabber {
	std::string name;
	int FIO_suction;
	int FIO_actuation;
	double x_offset, y_offset, z_absolute; // Z height doesn't change for lid grabbing so thats why it is an absolute value and not an offset
	std::vector<int> must_use_for_cols; // Due to the X range of the robot, the leftmost columns and rightmost columns may only be accessible by
										// one lid grabber. i.e. The robot can 't move far enough to the left to position the lid grabber on the
										// right to grab lids in the leftmost column, so we MUST use the left lid gabber for the leftmost column. 
										// If that is the case the columns exclusive to this lid grabber will be stored here.
	
	bool is_holding_lid = false;
	MatIndex holding_lid_of_plate = MatIndex(-1, -1);

	void liftOrDropLid(OxCnc& Ox);
};

//enum plate_type {IS_SOURCE, IS_DESTINATION};

// TODO: Remove all external lid handling techniques so that all lid handling happens through this class.
class LidHandler {
	public:
		enum plate_type { IS_SOURCE, IS_DESTINATION };

		LidHandler();
		void manageLids(MatIndex plate_loc, PlateTray& Tr, OxCnc& Ox);
		void setPlateType(MatIndex plate_loc, plate_type type);
		void dropAllLids(PlateTray& Tr, OxCnc& Ox);
		void liftOrDropWithFirstGrabber(OxCnc& Ox);

	protected:
		std::string getPlateID(MatIndex plate_loc);
		cv::Point3d getLidGrabberPosition(LidGrabber& grabber, cv::Point3d plate_coords);
		bool moveGrabberIntoPosition(LidGrabber& grabber, MatIndex target_plate, PlateTray& Tr, OxCnc& Ox);
		

		std::vector<LidGrabber> grabbers;
		//std::unordered_map<std::string, plate_type> plate_types;
		std::unordered_map<MatIndex, plate_type> plate_types;

};