/*
	LidHandler.cpp
	Peter Bowlin
	August 2021


	This class handles everything that has to do with managing lids for the robot
*/

#pragma once
#include "LidHandler.h"
#include "Config.h"
#include "json.hpp"


using namespace std;
using namespace cv;
using json = nlohmann::json;

void LidGrabber::liftOrDropLid(OxCnc& Ox) {
	bool dropping = is_holding_lid;

	// Lower the gripper
	LabJack_SetFioN(Ox.getLabJackHandle(), FIO_actuation, 0);
	//boost_sleep_ms(1500);

	// Activate/deactivate suction
	LabJack_SetFioN(Ox.getLabJackHandle(), FIO_suction, dropping);


	boost_sleep_ms(500);
	// Raise lid gripper
	LabJack_SetFioN(Ox.getLabJackHandle(), FIO_actuation, 1);
	boost_sleep_ms(500);

	is_holding_lid = !dropping;
}

LidHandler::LidHandler() {
	// Load the file from disk
	json config = Config::getInstance().getConfigTree("lid grabbers");

	// Get the information for each lid grabber from the JSON
	for (auto grabber : config) {
		LidGrabber lid_grabber;
		lid_grabber.name = grabber["name"].get<string>();
		lid_grabber.FIO_actuation = grabber["FIO actuation"].get<int>();
		lid_grabber.FIO_suction = grabber["FIO suction"].get<int>();
		lid_grabber.x_offset = grabber["x offset"].get<double>();
		lid_grabber.y_offset = grabber["y offset"].get<double>();
		lid_grabber.z_absolute = grabber["z absolute"].get<double>();

		for (auto col : grabber["must use for cols"]) {
			lid_grabber.must_use_for_cols.push_back(col);
		}

		grabbers.push_back(lid_grabber);
	}

	// Load each plate from the JSON into the map, and set the type to source by default. 
	json plates_config = Config::getInstance().getConfigTree("plates");
	for (auto plate_json : plates_config["plates"]) {
		int r, c;
		r = plate_json["row"].get<int>();
		c = plate_json["col"].get<int>();

		//string plate_ID = getPlateID(MatIndex(r, c));

		plate_types[MatIndex(r,c)] = IS_SOURCE;
	}

	return;
}


/*
This will "intelligently" manage the plate lids. 
	It attempts to increase efficiency by looking to see which plates are classified as source plates and which are classified as destination
	plates prior to making a decision on how to handle the lids.
	Note that all plates are source plates by default, and this will still work if there are only source plates. But if you set your destination plates
	as such in your high level script, then it will more intelligently manage the lids.
*/
void LidHandler::manageLids(MatIndex plate_loc, PlateTray& Tr, OxCnc& Ox) {
	
	// If we are already holding the lid then we don't need to do anything
	for (LidGrabber& grabber : grabbers) {
		if (grabber.is_holding_lid && grabber.holding_lid_of_plate == plate_loc) return;
	}

	LidGrabber* grabber_to_use = &grabbers[0]; 
	bool exclusive_col = false;

	// First check to see if the column we are moving to is exclusive to one of our lid grabbers
	for (LidGrabber& grabber : grabbers) {
		for (int col : grabber.must_use_for_cols) {
			if (col == plate_loc.col) {
				grabber_to_use = &grabber;
				exclusive_col = true;
				cout << "Plates in column " << col << " are exclusive to grabber: " << grabber.name << ". So using that grabber." << endl;
				break;
			}
			if (exclusive_col) break;
		}
	}

	if (!exclusive_col) {
		bool open_grabber = false;
		// Check to see if we have any currently open grabbers
		for (LidGrabber& grabber : grabbers) {
			if (!grabber.is_holding_lid) {
				grabber_to_use = &grabber;
				open_grabber = true;
				break;
			}
		}

		if (!open_grabber) {
			//string plate_ID = getPlateID(plate_loc);
			for (int i = 0; i < grabbers.size(); i++) {
				//string grabber_plate_ID = getPlateID(grabbers[i].holding_lid_of_plate);
				if (plate_types[plate_loc] == plate_types[grabbers[i].holding_lid_of_plate] || i == grabbers.size() - 1) {
					grabber_to_use = &grabbers[i];
					break;
				}
			}
		}
	}

	// Print out the grabber to use's properties
	/*cout << "Grabber to use: " << grabber_to_use->name << endl;
	cout << "\tIs holding a lid: " << grabber_to_use->is_holding_lid << endl;
	cout << "\tPlate (" << grabber_to_use->holding_lid_of_plate.row << ", " << grabber_to_use->holding_lid_of_plate.col << ")" << endl;*/

	// If we are using a grabber that is already holding a lid then we first must drop that lid
	if (grabber_to_use->is_holding_lid) {
		// Move the grabber into position to drop the lid
		moveGrabberIntoPosition(*grabber_to_use, grabber_to_use->holding_lid_of_plate, Tr, Ox);

		// Drop the lid
		grabber_to_use->liftOrDropLid(Ox);
	}

	// Grab the lid of the target plate
	moveGrabberIntoPosition(*grabber_to_use, plate_loc, Tr, Ox);
	grabber_to_use->liftOrDropLid(Ox);
	grabber_to_use->holding_lid_of_plate = plate_loc;
	
	/*cout << "after maneuvers" << endl;
	cout << "Grabber to use: " << grabber_to_use->name << endl;
	cout << "\tIs holding a lid: " << grabber_to_use->is_holding_lid << endl;
	cout << "\tPlate (" << grabber_to_use->holding_lid_of_plate.row << ", " << grabber_to_use->holding_lid_of_plate.col << ")" << endl;*/

	/*cout << endl;
	for (LidGrabber& grabber : grabbers) {
		cout << "Grabber to use: " << grabber.name << endl;
		cout << "\tIs holding a lid: " << grabber.is_holding_lid << endl;
		cout << "\tPlate (" << grabber.holding_lid_of_plate.row << ", " << grabber.holding_lid_of_plate.col << ")" << endl;
		cout << endl;
	}

	cout << "+++++++++++++++++++" << endl;*/

}

// This will drop any lids that are currently being held by grabbers back onto their corresponding plate.
// For instance, at the end of an experiment.
void LidHandler::dropAllLids(PlateTray& Tr, OxCnc& Ox) {
	for (LidGrabber& grabber : grabbers) {
		
		if (grabber.is_holding_lid) {
			cout << "Holding..." << endl;
			// Move the grabber into position to drop the lid
			moveGrabberIntoPosition(grabber, grabber.holding_lid_of_plate, Tr, Ox);

			// Drop the lid
			grabber.liftOrDropLid(Ox);
		}
	}
}

// Special method just for running manual lifting and dropping of lids from wormpicker main loop, when grabber is already positioned above the desired plate.
// This should not be called from an automated script
void LidHandler::liftOrDropWithFirstGrabber(OxCnc& Ox) {
	Point3d orig_position = Ox.getXYZ();

	// Move the robot to the appropriate height for grabbing
	Point3d position = Ox.getXYZ();
	position.z = grabbers[0].z_absolute;
	Ox.goToAbsolute(position, true);

	// Handle the lid
	grabbers[0].liftOrDropLid(Ox);

	// Move back to the original position
	Ox.goToAbsolute(orig_position);
}

void LidHandler::setPlateType(MatIndex plate_loc, plate_type type) {
	//string plate_ID = getPlateID(plate_loc);
	plate_types[plate_loc] = type;
}

/*
Returns the key for the given plate location in the plate_types map. The reason we add 1000 to the value for the key is to ensure all the keys are unique for our tray
Example: without adding 1000 the key for row 1 col 10 is "110"... but this would be the same key for row 11 col 0. Adding 1000 avoids this issue.
DEPRECATED: I made MatIndex hashable so now we can just use the MatIndex as the key.
*/
string LidHandler::getPlateID(MatIndex plate_loc) {
	return to_string(plate_loc.row + 1000) + to_string(plate_loc.col + 1000);
}


Point3d LidHandler::getLidGrabberPosition(LidGrabber& grabber, Point3d plate_coords) {
	return Point3d(plate_coords.x + grabber.x_offset, plate_coords.y + grabber.y_offset, grabber.z_absolute); 
}

bool LidHandler::moveGrabberIntoPosition(LidGrabber& grabber, MatIndex target_plate, PlateTray& Tr, OxCnc& Ox) {
	Tr.changeScriptId(target_plate);
	Tr.updateStepInfo();

	Point3d lid_pos_coords = getLidGrabberPosition(grabber, Tr.getCurrentPosition());

	// Move the grabber into position to drop
	if (!Ox.goToAbsolute(lid_pos_coords, true)) {
		cout << "goToAbsolute failed!" << endl;
		return false;
	} /// Exit if failed

	return true;
}

