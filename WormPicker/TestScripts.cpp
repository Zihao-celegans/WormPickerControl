/*
	Rules for test scripts:
	
	1. function name should start with initials like "JL_AssessHeadTail()"

	2. Use modules we have already for functionality that already exists, like segmentWorm for
		segmenting and measuing fluorescence; avoid creating many similar test functions


*/

#include "TestScripts.h"
#include <string>
#include <vector>
#include <direct.h>
#include "WormFinder.h"
#include "Worm.h"
#include "showDebugImage.h"
#include "PlateTray.h"
#include "OxCnc.h"
#include "ScriptActions.h"
#include "mask_rcnn.h"
#include "AnthonysCalculations.h"
#include "ImageFuncs.h"
#include "DirList.h"
#include <boost/filesystem.hpp>
#include <regex>
#include "ControlledExit.h"


// OpenCV includes
#include "opencv2\opencv.hpp"

using namespace std;
using namespace cv;
namespace filesys = boost::filesystem;


/*
	John's test scripts
*/


// Action script that find the larva in the FOV, move to high-mag cam, and move to pick up location, if the larva stage is what you want
bool testCenterLarva(WormPick &Pk, PlateTray &Tr, OxCnc &Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder& worms, int& kg) {

	// args[5] is 0 - Identify larva stage by automatic assessment
	// args[5] is 1 - Identify larva stage by human assessment
	bool assess_mode = stoi(Tr.getCurrentScriptStepInfo().args[5]) != 0;

	// args[4] is 0 - Egg; 1 - L1_LARVA; 2 - L2_LARVA; 3 - L3_LARVA; 4 - L4_LARVA; 5 - Adult
	int larval_stage = stoi(Tr.getCurrentScriptStepInfo().args[4]);

	// Temporialy set the pickup location to the center of fluo FOV
	Point orig_pickup_loc = worms.getPickupLocation();
	worms.setPickupLocation(287, 200);

	// Get the worm's position in the FOV
	vector<Point> FOV_posit;

	for (Worm& w : worms.all_worms) {
		FOV_posit.push_back(w.getWormCentroid());
	}

	// Rearrange the order in worm's position vector, center the worm that is cloest to the fluo camera first
	compDist compObject; compObject.ref_pt = worms.getPickupLocation();
	sort(FOV_posit.begin(), FOV_posit.end(), compObject);

	// Convert the FOV coordinates to plate coordinate
	Point3d stage_coord(Ox.getX(), Ox.getY(), Ox.getZ());
	vector<Point3d> abs_worm_coord = convertCoord(FOV_posit, worms.getPickupLocation(), stage_coord, Pk.dydOx); // To change

	// Go to the plate coordinate of each worm
	bool larva_found = false;
	for (int i = 0; i < abs_worm_coord.size(); i++) {

		AnthonysTimer Ta;
		double time_limit_s = 3;

		Ox.goToAbsolute(abs_worm_coord[i].x, abs_worm_coord[i].y, abs_worm_coord[i].z, true, 5);

		// Toggle the worm-finding mode
		worms.worm_finding_mode = 1;
		worms.trackwormNearPcikupLoc = 1;
		boost_sleep_ms(200);

		// track the worm and move to fluo cam
		worms.trackwormNearPcikupLoc = 0;
		worms.getWormNearestPickupLocCNN();
		helperKeepWormCentered(Ta, time_limit_s, worms, Ox);


		// Get the metric of the worm's contour, e.g. area
		float worm_area = worms.getTrackedWorm()->getAreaV2();
		cout << "Area of worm contour: " << worm_area << endl;

		// See whether meet the criteria
		if (larval_stage == 0) { larva_found = (worm_area < 160 && worm_area > 0); }
		else if (larval_stage == 5) { larva_found = (worm_area > 160); }

		// (worm_area < 180 && worm_area > 0);
		cout << "Area within the range?	" << larva_found << endl;

		if (assess_mode) {
			string input_fragment;
			cout << "Correct larva?    1(Yes)  -or-  ENTER(No)...\n\n";
			getline(cin, input_fragment);
			if (input_fragment.size() > 0) { larva_found = true; }
			else { larva_found = false; }
		}

		// If it lies within the range you want, stop the loop and move to pickup location
		if (larva_found) {
			cout << "Larva found!" << endl;
			break;
		}
		else {
			cout << "The larval stage is not correct, finding the next worm..." << endl;
		}
	}

	// Set the pick up location back to the original true value
	worms.setPickupLocation(orig_pickup_loc.x, orig_pickup_loc.y);

	// center the targeted larva to pickup location
	if (larva_found) {
		AnthonysTimer Tb;
		double time_limit_s = 25;
		worms.getWormNearestPickupLocCNN();
		helperCenterWormCNNtrack(Tb, time_limit_s, worms, Ox);
	}
	else {
		cout << "Cannot find the desired larva!" << endl;
	}

	return larva_found;
}



bool executeFluoImg(WormPick &Pk, const PlateTray& Tr, OxCnc& Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, CameraFactory& camFact) {

	// Test function for enabling fluorescence imaging mode
	// FIO7 control the ON/OFF status of white illumination;
	// FIO4 control the ON/OFF status of blue LED

	boost_sleep_ms(10000);

	// Get the pointer to access the fluo camera
	shared_ptr<AbstractCamera> fluoCam = camFact.Nth_camera(1);
	shared_ptr<StrictLock<AbstractCamera>> fluoCamLock = camFact.getCameraLock(fluoCam);

	int expo_time = stoi(Tr.getCurrentScriptStepInfo().args[5]);

	// Turn OFF the white illumination
	LabJack_SetFioN(Grbl2.getLabJackHandle(), 7, 0);

	// Turn on the blue LED excitation light
	LabJack_SetFioN(Grbl2.getLabJackHandle(), 4, 1);

	boost_sleep_ms(500);


	// Get the current exposure time
	long long origl_expo = fluoCam->getExposureTime(*fluoCamLock);

	cout << "origl_expo = " << origl_expo << endl;

	// Adjust the exposure time of fluorescence camera
	fluoCam->setExposureTime(*fluoCamLock, 1000 * expo_time);

	boost_sleep_ms(5000);

	// Adjust the exposure time of fluorescence camera back to low value
	fluoCam->setExposureTime(*fluoCamLock, 2500);

	// Turn the blue LED back to Off
	LabJack_SetFioN(Grbl2.getLabJackHandle(), 4, 0);
	boost_sleep_ms(500);

	// Turn the white illumination back to ON
	LabJack_SetFioN(Grbl2.getLabJackHandle(), 7, 1);
	boost_sleep_ms(500);

	return true;

}



// TO DO: Modulate the code with centerlarva
bool executeCenterFluoWorm(WormPick &Pk, const PlateTray& Tr, OxCnc& Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder& worms, CameraFactory& camFact) {

	// args[4] is 0 - find worm WITHOUT fluorescence; 1 - find worm w/ fluorescence
	bool have_fluo = stoi(Tr.getCurrentScriptStepInfo().args[4]) != 0;

	// args[5] is exposure time (ms) set to fluorescence camera to image fluorescent worm
	//double expo_time = Tr.getCurrentScriptStepInfo().args[5];

	// Temporialy set the pickup location to the center of fluo FOV
	Point orig_pickup_loc = worms.getPickupLocation();
	Point ctr_ovlpFov = worms.getCtrOvlpFov();
	worms.setPickupLocation(ctr_ovlpFov.x, ctr_ovlpFov.y);

	// Get the worm's position in the FOV
	vector<Point> FOV_posit;

	for (Worm& w : worms.all_worms) {
		FOV_posit.push_back(w.getWormCentroid());
	}

	// Rearrange the order in worm's position vector, center the worm that is cloest to the fluo camera first
	compDist compObject; compObject.ref_pt = worms.getPickupLocation();
	sort(FOV_posit.begin(), FOV_posit.end(), compObject);

	// Convert the FOV coordinates to plate coordinate
	Point3d stage_coord(Ox.getX(), Ox.getY(), Ox.getZ());
	vector<Point3d> abs_worm_coord = convertCoord(FOV_posit, worms.getPickupLocation(), stage_coord, Pk.dydOx); // To change

	// Go to the plate coordinate of each worm
	bool worm_found = false;
	for (int i = 0; i < abs_worm_coord.size(); i++) {

		AnthonysTimer Ta;
		double time_limit_s = 3;

		Ox.goToAbsolute(abs_worm_coord[i].x, abs_worm_coord[i].y, abs_worm_coord[i].z, true, 5);

		// Toggle the worm-finding mode
		worms.worm_finding_mode = 1;
		worms.trackwormNearPcikupLoc = 1;
		boost_sleep_ms(200);

		// track the worm and move to fluo cam
		worms.trackwormNearPcikupLoc = 0;
		worms.getWormNearestPickupLocCNN();
		helperKeepWormCentered(Ta, time_limit_s, worms, Ox);

		// Switch to fluorescence imaging mode
		ImagingMode(GFP_IMAGING, Grbl2, camFact);

		// Wait one second
		//boost_sleep_ms(1000);

		// Get the metric of the fluorescence image
		float fluore_area = worms.getHiMagWorm().getAreaV2();
		cout << "Area of fluo contour: " << fluore_area << endl;

		// See whether meet the criteria
		if (have_fluo == 0) { worm_found = (fluore_area < 50 && fluore_area >= 0); }
		else if (have_fluo == 1) { worm_found = (fluore_area >= 50 && fluore_area < 1000); }

		cout << "Area within the range?	" << worm_found << endl;

		// Switch back to brightfield imaging mode
		ImagingMode(BRIGHTFIELD, Grbl2, camFact); // TO DO: get the original exposure time from camera

		// If it lies within the range you want, stop the loop and move to pickup location
		if (worm_found) {
			cout << "Fluo Worm found!" << endl;
			break;
		}
		else {
			cout << "This worm is not fluorescent, finding the next worm..." << endl;
		}
	}

	// Set the pick up location back to the original true value
	worms.setPickupLocation(orig_pickup_loc.x, orig_pickup_loc.y);

	// center the targeted larva to pickup location
	if (worm_found) {
		AnthonysTimer Tb;
		double time_limit_s = 25;
		worms.getWormNearestPickupLocCNN();
		helperCenterWormCNNtrack(Tb, time_limit_s, worms, Ox);
	}
	else {
		cout << "Cannot find the fluorescent worm!" << endl;
	}

	return worm_found;
}


//bool executeMeasureFluo(WormPick &Pk, const PlateTray& Tr, OxCnc& Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder& worms, CameraFactory& camFact) {
//
//	// args[2] is the time latency (100*ms) of switching brightfield and fluorescence imaging
//	double delay = Tr.getCurrentScriptStepInfo().args[2];
//
//	// args[3] is the binary indicator: whether write the data to file
//	bool write_data = Tr.getCurrentScriptStepInfo().args[3];
//
//	// args[4] is the bin number of segmentation along the body coordinate
//	int bin_num = Tr.getCurrentScriptStepInfo().args[4];
//
//	// args[5] is exposure time (ms) set to fluorescence camera to image fluorescent worm
//	int expo_time = Tr.getCurrentScriptStepInfo().args[5];
//
//	Mat frm_before_fluo, frm_fluo;
//	// Get the brightfield frame right before the fluorescence imaging
//	worms.img_high_mag.copyTo(frm_before_fluo);
//
//	// Enable fluorescence imaging
//	ImagingMode(1, expo_time, Grbl2, camFact);
//
//	boost_sleep_ms(100 * delay + expo_time);
//
//	// Get the fluorescence frame
//	worms.img_high_mag.copyTo(frm_fluo);
//
//	// Turn back to brightfield imaging
//	ImagingMode(0, 5, Grbl2, camFact);
//
//	// Measure the fluorescence signal along the worm body
//	ControlPanel CP0(worms.img_high_mag.size(), ""); worms.setFluoCamSegWormWholeFOV(CP0);
//	vector<double> fluo_body_coord;
//	//worms.MeasureFluo(CP0, frm_fluo, frm_before_fluo, bin_num, fluo_body_coord);
//
//	// Write the fluorescence signal along the body coordinate to file
//	if (write_data && !fluo_body_coord.empty()) {
//
//		// confirm by user
//		bool confirm_write = false;
//		string input_fragment;
//		cout << "Write to file?    0(N)  -or-  ENTER(Y)...\n\n";
//		getline(cin, input_fragment);
//		if (input_fragment.size() == 0) { confirm_write = true; }
//
//		if (confirm_write) {
//
//			// Flip the head and tail?
//			bool flip = false;
//			string input_fragment_2;
//			cout << "Flip the head and tail?    0(N)  -or-  ENTER(Y)...\n\n";
//			getline(cin, input_fragment_2);
//			if (input_fragment_2.size() == 0) {
//				flip = true;
//			}
//
//			if (flip) {
//				reverse(fluo_body_coord.begin(), fluo_body_coord.end());
//			}
//
//			cout << "Write the fluorescence signal along the body coordinate to file..." << endl;
//			ofstream fluodata;
//			fluodata.open("C:\\WormPicker\\Fluorescence_data\\YX261_20200908\\YX261.txt", std::fstream::app);
//			if (fluodata.is_open()) {
//				for (int i = 0; i < fluo_body_coord.size(); i++) {
//					fluodata << round(fluo_body_coord[i]) << "\t";
//				}
//				fluodata << endl;
//				fluodata.terminate();
//			}
//			else {
//				cout << "Unable to open file" << endl;
//			}
//		}
//
//	}
//
//	return true;
//}


//// Measure the metrics about head and tail under high_mag camera for classification purpose
//bool executeMeasureHeadTail(WormPick &Pk, const PlateTray& Tr, OxCnc& Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder& worms) {
//
//	// args[5] is the binary indicator: whether write the data to file
//	bool write_data = Tr.getCurrentScriptStepInfo().args[5];
//
//	// args[4] the waiting time (in 100ms) between two frames 
//	int wait_time = Tr.getCurrentScriptStepInfo().args[4];
//
//	// args[3] the length of head and tail region
//	double length = Tr.getCurrentScriptStepInfo().args[3];
//
//	// args[2] the width of head and tail region
//	double width = Tr.getCurrentScriptStepInfo().args[2]; // Default value: 0.45
//
//
//	// Get one frame
//	Mat pre_frm; worms.img_high_mag.copyTo(pre_frm);
//
//
//	// Get another frame
//	boost_sleep_ms(100 * wait_time);
//	Mat cur_frm; worms.img_high_mag.copyTo(cur_frm);
//
//	// Measure the metrics of head and tail
//	ControlPanel CP0(worms.img_high_mag.size(), ""); worms.setFluoCamSegWormWholeFOV(CP0);
//	pair<double, double> angles = pair<double, double>(-1, -1), movement = pair<double, double>(-1, -1), intensity = pair<double, double>(-1, -1);
//
//	vector<double> body_intensity;
//	//worms.AssessHeadTail(CP0, cur_frm, pre_frm, angles, movement, intensity, body_intensity, length, width);
//
//	// Write the fluorescence signal along the body coordinate to file
//	if (write_data && angles != pair<double, double>(-1, -1) && movement != pair<double, double>(-1, -1) && intensity != pair<double, double>(-1, -1)) {
//
//		// confirm by user
//		bool confirm_write = false;
//		string input_fragment;
//		cout << "Write to file?    0(N)  -or-  ENTER(Y)...\n\n";
//		getline(cin, input_fragment);
//		if (input_fragment.size() == 0) { confirm_write = true; }
//
//		if (confirm_write) {// Flip the head and tail?
//			bool flip = false;
//			string input_fragment_2;
//			cout << "Flip the head and tail?    0(N)  -or-  ENTER(Y)...\n\n";
//			getline(cin, input_fragment_2);
//			if (input_fragment_2.size() == 0) {
//				flip = true;
//			}
//
//			if (flip) {
//				// flip the metrics of head and tail
//				angles = pair<double, double>(angles.second, angles.first);
//				movement = pair<double, double>(movement.second, movement.first);
//				intensity = pair<double, double>(intensity.second, intensity.first);
//
//				reverse(body_intensity.begin(), body_intensity.end());
//			}
//
//			cout << "Write the metrics of head and tail to file..." << endl;
//
//			ofstream angledata, movementdata, intensitydata, bodyintensitydata;
//			angledata.open("C:\\WormPicker\\HeadTail\\20200916\\Angle.txt", std::fstream::app);
//			movementdata.open("C:\\WormPicker\\HeadTail\\20200921\\Movement.txt", std::fstream::app);
//			intensitydata.open("C:\\WormPicker\\HeadTail\\20200916\\Intensity.txt", std::fstream::app);
//			bodyintensitydata.open("C:\\WormPicker\\HeadTail\\20201003\\Male_intensity.txt", std::fstream::app);
//
//
//			if (angledata.is_open() && movementdata.is_open() && intensitydata.is_open() && bodyintensitydata.is_open()) {
//				//angledata << angles.first << "\t"; angledata << angles.second << "\t"; angledata << endl; angledata.terminate();
//				//movementdata << movement.first << "\t"; movementdata << movement.second << "\t"; movementdata << endl; movementdata.terminate();
//				//intensitydata << intensity.first << "\t"; intensitydata << intensity.second << "\t"; intensitydata << endl; intensitydata.terminate();
//
//				for (int i = 0; i < body_intensity.size(); i++) {
//					bodyintensitydata << round(body_intensity[i]) << "\t";
//				}
//				bodyintensitydata << endl;
//				bodyintensitydata.terminate();
//			}
//			else {
//				cout << "Unable to open file !" << endl;
//			}
//		}
//	}
//
//	return true;
//}

bool executeMeasrPixDiff(WormPick& Pk, const PlateTray& Tr, OxCnc& Ox, OxCnc& Grbl2, StrictLock<OxCnc>& oxLock, StrictLock<OxCnc>& grbl2Lock, WormFinder& worms, CameraFactory& camFact) {

	int num_record = 5;

	// STEP 1: Get the plate parameters from config file
	Point3d PlatePos = Tr.getCurrentPosition(); // TO DO: Get from config 
	float Diameter = 60; // TO DO: Get from config
	float FOV = 12; // TO DO: Get from config

	PlateMap plate_map(FOV, Diameter, PlatePos); // construct a PlateMap object for this plate

	// STEP 2: Start the gantry movement
	//MoveandLift first
	PlateTray dummy_tray(Tr);
	dummy_tray.changeScriptArgs({ "1", "0", "0", "1", "1", "0", "0", "0", "0", "0" });
	if (!executeMoveToPlate(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact)) { return false; }

	// Lock CNC
	oxLock.lock();

	bool success = false;

	// move the gantry right above the plate center
	success = Ox.goToAbsolute(PlatePos, true, 25);
	if (!success) {

		// Unlcok CNC
		oxLock.unlock();
		return success; /// Exit if failed
	}

	plate_map.CalScanPoint(); // Set the scan points

	// vector for recording the pixel diff for tracked worms

	vector<int> vec_pixDiff;
	vector<double> vec_conf;

	// STEP 3: Move the camera in a spiral manner to scan for worms over the whole plate
	for (int i = 0; i < (plate_map.getScanCoordPair()).size(); i++) { // iterate through all the entries of PlateMap

		// Go to the scan point
		if (!Ox.goToAbsolute((plate_map.getScanCoordPair()[i]).first, true, 25)) {
			continue;
		}

		// Wait for several seconds to let diff image settle down
		boost_sleep_ms(7000);

		worms.findWorminWholeImg = false;

		for (int j = 0; j < num_record; j++) {
			vector<int> this_pixDif = worms.getPixelDiff();
			vector<double> this_conf = worms.getConfiSinglefrmCnn();

			if (this_pixDif.empty() || this_conf.empty()) { continue; }

			for (int m = 0; m < this_pixDif.size(); m++) {
				vec_pixDiff.push_back(this_pixDif[m]);
			}

			for (int n = 0; n < this_conf.size(); n++) {
				vec_conf.push_back(this_conf[n]);
			}

			boost_sleep_ms(3000);
		}

		worms.findWorminWholeImg = true;
	}

	// Print out statistics

	std::cout << "---------------Pixel Change---------------" << endl;
	std::cout << "Num of data point = " << vec_pixDiff.size() << endl;
	std::cout << "Mean = " << vectorMean(vec_pixDiff) << endl;
	std::cout << "Standard Deviation = " << vectorStd(vec_pixDiff) << endl << endl;

	std::cout << "---------------Confidence Level---------------" << endl;
	std::cout << "Num of data point = " << vec_conf.size() << endl;
	std::cout << "Mean = " << vectorMean(vec_conf) << endl;
	std::cout << "Standard Deviation = " << vectorStd(vec_conf) << endl;


	// Unlcok CNC
	oxLock.unlock();

	// TO DO: For demonstration: centering each the worm according to the plate coordinate
	return success;
}

// Action script that find the larva in the FOV, move to high-mag cam, and move to pick up location, if the larva stage is what you want
bool executeCenterLarva(WormPick &Pk, PlateTray &Tr, OxCnc &Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder &worms, int& kg) {

	// args[5] is 0 - Identify larva stage by automatic assessment
	// args[5] is 1 - Identify larva stage by human assessment
	bool assess_mode = stoi(Tr.getCurrentScriptStepInfo().args[5]) != 0;

	// args[4] is 0 - Egg; 1 - L1_LARVA; 2 - L2_LARVA; 3 - L3_LARVA; 4 - L4_LARVA; 5 - Adult
	int larval_stage = stoi(Tr.getCurrentScriptStepInfo().args[4]);

	// Temporialy set the pickup location to the center of fluo FOV
	Point orig_pickup_loc = worms.getPickupLocation();
	worms.setPickupLocation(287, 200);

	// Get the worm's position in the FOV
	vector<Point> FOV_posit;

	for (Worm& w : worms.all_worms) {
		FOV_posit.push_back(w.getWormCentroid());
	}

	// Rearrange the order in worm's position vector, center the worm that is cloest to the fluo camera first
	compDist compObject; compObject.ref_pt = worms.getPickupLocation();
	sort(FOV_posit.begin(), FOV_posit.end(), compObject);

	// Convert the FOV coordinates to plate coordinate
	Point3d stage_coord(Ox.getX(), Ox.getY(), Ox.getZ());
	vector<Point3d> abs_worm_coord = convertCoord(FOV_posit, worms.getPickupLocation(), stage_coord, Pk.dydOx); // To change

	// Go to the plate coordinate of each worm
	bool larva_found = false;
	for (int i = 0; i < abs_worm_coord.size(); i++) {

		AnthonysTimer Ta;
		double time_limit_s = 3;

		Ox.goToAbsolute(abs_worm_coord[i].x, abs_worm_coord[i].y, abs_worm_coord[i].z, true, 5);

		// Toggle the worm-finding mode
		worms.worm_finding_mode = 1;
		worms.trackwormNearPcikupLoc = 1;
		boost_sleep_ms(200);

		// track the worm and move to fluo cam
		worms.trackwormNearPcikupLoc = 0;
		worms.getWormNearestPickupLocCNN();
		helperKeepWormCentered(Ta, time_limit_s, worms, Ox);


		// Get the metric of the worm's contour, e.g. area
		float worm_area = worms.getTrackedWorm()->getAreaV2();
		cout << "Area of worm contour: " << worm_area << endl;

		// See whether meet the criteria
		if (larval_stage == 0) { larva_found = (worm_area < 160 && worm_area > 0); }
		else if (larval_stage == 5) { larva_found = (worm_area > 160); }

		// (worm_area < 180 && worm_area > 0);
		cout << "Area within the range?	" << larva_found << endl;

		if (assess_mode) {
			string input_fragment;
			cout << "Correct larva?    1(Yes)  -or-  ENTER(No)...\n\n";
			getline(cin, input_fragment);
			if (input_fragment.size() > 0) { larva_found = true; }
			else { larva_found = false; }
		}

		// If it lies within the range you want, stop the loop and move to pickup location
		if (larva_found) {
			cout << "Larva found!" << endl;
			break;
		}
		else {
			cout << "The larval stage is not correct, finding the next worm..." << endl;
		}
	}

	// Set the pick up location back to the original true value
	worms.setPickupLocation(orig_pickup_loc.x, orig_pickup_loc.y);

	// center the targeted larva to pickup location
	if (larva_found) {
		AnthonysTimer Tb;
		double time_limit_s = 25;
		worms.getWormNearestPickupLocCNN();
		helperCenterWormCNNtrack(Tb, time_limit_s, worms, Ox);
	}
	else {
		cout << "Cannot find the desired larva!" << endl;
	}

	return larva_found;
}


bool executeLowerPicktoTouch(WormPick &Pk, PlateTray &Tr, OxCnc &Ox, OxCnc &Grbl2, WormFinder &worms, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, CameraFactory& camFact) {
	int trials = stoi(Tr.getCurrentScriptStepInfo().args[0]); // Number of times to lower & raise the pick
	PlateTray dummy_tray(Tr);
	// Move to the plate we want to test on
	dummy_tray.changeScriptArgs({ "1", "1", "0", "0", "0", "0", "0", "0", "0", "0" }); // set move inputs
	executeMoveToPlate(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);

	// Move pick to right above focus height

	for (int i = 0; i < trials; i++) {

		cout << "*********** Start Trial " << i << " *******************"<< endl;


		

		//Grbl2.goToAbsolute(Grbl2.getX(), Grbl2.getY(), Pk.getPickupHeight() - Pk.pick_focus_height_offset, true, 0);

		// Move the pick to center position
		//Pk.movePickToStartPos(Grbl2, 1, 0, 0, 0);
		Pk.readyPickToStartPosition(Grbl2);

		// Record the height of grbl2 before lowering
		double old_Z = Pk.getActx().readPosition();

		// Lower the pick until touch
		Pk.lowerPickToTouch(Grbl2, Grbl2.getLabJackHandle(), false);
		cout << "Done lowering Pick." << endl;
		imshow("Waiting", worms.img_high_mag_color);
		waitKey(0);
		//Pk.getActx().moveAbsolute(1900);
		
		//// Pick a worm
		//Pk.pickOrDropOneWorm(Ox, Grbl2, worms, 2, 15, 0, false, false,(pick_mode)0, getPlateIdKey(dummy_tray));


		// Move pick right above the plate
		Pk.movePickSlightlyAbovePlateHeight();

		//dummy_tray.changeScriptArgs({ 1, 0, 1, 0, 0, 0, 0, 0, 0, 0 }); // set move inputs
		//executeMoveToPlate(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock);

		//// Drop the worm
		//Pk.pickOrDropOneWorm(Ox, Grbl2, worms, 2, -12, 3, false, false, (pick_mode)0);



		//// Go to next plate
		//int source_col = i % 2 == 0 ? 2 : 1;

		//MatIndex pickid = MatIndex(5, source_col);
		//dummy_tray.changeScriptId(pickid);
		//dummy_tray.updateStepInfo();


		//executeMoveToPlate(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock);



		// Record the height of grbl2 after lowering
		double new_Z = Pk.getActx().readPosition();

		cout << "old Z: " << old_Z << endl;
		cout << "New Z: " << new_Z << endl;

		/*if ( new_Z - old_Z >= 700) {

			cout << "Lowered too much, capacitive sensor failed!" << endl;
			break;
		}*/


		//boost_sleep_ms(1500);
	}


	//Raise pick back up when done.
	//Grbl2.goToAbsolute(Grbl2.getX(), Grbl2.getY(), Pk.getPickupHeight() - Pk.pick_focus_height_offset, true, 0);
	Pk.getActx().moveAbsolute(200);

	return true;
}

bool executeMoveOnClick(WormPick &Pk, OxCnc &Ox, WormFinder& worms) {

	Point old_pos_to_move = worms.loc_to_move_user_set;

	cout << "Please click on Fig. 1 to set the position to move..." << endl;

	// Wait for user click
	while (worms.loc_to_move_user_set.x == old_pos_to_move.x && 
		worms.loc_to_move_user_set.y == old_pos_to_move.y) {
		boost_sleep_ms(300);
	}

	// Center the worm in the high mag field of view
	double dx = (worms.loc_to_move_user_set.x - worms.high_mag_fov_center_loc.x) / Pk.dydOx;
	double dy = (worms.loc_to_move_user_set.y - worms.high_mag_fov_center_loc.y) / Pk.dydOx;

	cout << "Moving to click position" << endl;
	Ox.goToRelative(dx, dy, 0, 1.0, true, 0);

	// Set the loc_to_move_user_set back to the center of image
	worms.loc_to_move_user_set = old_pos_to_move;

	return true;
}

bool executePickOnClick(WormPick &Pk, PlateTray &Tr, OxCnc &Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder& worms, CameraFactory& camFact, int& kg) {


	// args[0]: number: amount of worms to sort
	int num_worms_to_sort = stoi(Tr.getCurrentScriptStepInfo().args[0]);

	// Source plate coordinate
	int source_row = Tr.getCurrentScriptStepInfo().id.row;
	int source_col = Tr.getCurrentScriptStepInfo().id.col;

	// Destination plate coordinate
	int dst_row = stoi(Tr.getCurrentScriptStepInfo().args[2]);
	int dst_col = stoi(Tr.getCurrentScriptStepInfo().args[3]);

	int num_sorted_worms = 0;

	MatIndex pickid = Tr.getCurrentScriptStepInfo().id;
	PlateTray dummy_tray(Tr);

	AnthonysTimer step_timer;
	AnthonysTimer total_timer;

	// Put the pick at the start position
	Grbl2.goToAbsolute(Grbl2.getX(), Grbl2.getY(), Pk.getPickupHeight() - Pk.pick_focus_height_offset, true, 0);

	while (num_sorted_worms < num_worms_to_sort) {

		// Set the source plate id
		pickid = MatIndex(source_row, source_col);
		dummy_tray.changeScriptId(pickid);
		dummy_tray.updateStepInfo();

		step_timer.setToZero();
		total_timer.setToZero();

		// Go to the source plate
		dummy_tray.changeScriptArgs({ "1", "1", "0", "1", "0", "0", "0", "0", "0", "0" }); // set move inputs
		executeMoveToPlate(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);

		worms.image_timer.setToZero(); // We moved the stage so we must reset the image grabber timer for Wormfinder

		double step_time = step_timer.getElapsedTime();
		double total_time = total_timer.getElapsedTime();
		cout << "	Move to source: " << step_time << endl;
		step_timer.setToZero();

		// Find and center a worm
		dummy_tray.changeScriptArgs({ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }); // set centerworm inputs
		executeMoveOnClick(Pk, Ox, worms);

		step_time = step_timer.getElapsedTime();
		total_time = total_timer.getElapsedTime();
		cout << "	MoveOnClick: " << step_time << endl;

		//boost_sleep_ms(5000);
		step_timer.setToZero();

		// Pick the worm
		dummy_tray.changeScriptArgs({ "2", "15", "0", "0", "1", "1", "0", "0", "0", "0" }); // set pickworm inputs
		executeSpeedPick(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);

		step_time = step_timer.getElapsedTime();
		total_time = total_timer.getElapsedTime();
		cout << "	SpeedPick-Pick: " << step_time << endl;
		step_timer.setToZero();


		pickid = MatIndex(dst_row, dst_col);
		dummy_tray.changeScriptId(pickid);
		dummy_tray.updateStepInfo();
		dummy_tray.changeScriptArgs({ "1", "0", "1", "0", "0", "0", "0", "0", "0", "0" }); // set move inputs
		executeMoveToPlate(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);

		worms.image_timer.setToZero(); // We moved the stage so we must reset the image grabber timer for Wormfinder

		step_time = step_timer.getElapsedTime();
		total_time = total_timer.getElapsedTime();
		cout << "	MoveToPlate: " << step_time << endl;

		//boost_sleep_ms(5000);
		step_timer.setToZero();

		// Drop the worm at the destination plate
		dummy_tray.changeScriptArgs({ "1", "-12", "3", "0", "0", "1", "0", "0", "0", "0" }); // set pickworm inputs
		executeSpeedPick(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);

		step_time = step_timer.getElapsedTime();
		total_time = total_timer.getElapsedTime();
		cout << "	SpeedPick-Drop: " << step_time << endl;

		num_sorted_worms++;
		cout << "Worms sorted: " << num_sorted_worms << endl;

	}


	return true;
}



bool executePhtyFluoPlt(WormPick &Pk, PlateTray &Tr, OxCnc &Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc>& grbl2Lock, WormFinder& worms, CameraFactory& camFact) {

	// Number of worm to check
	int num_worms = stoi(Tr.getCurrentScriptStepInfo().args[0]);

	// Exposure time for fluo imaging
	//int exp = Tr.getCurrentScriptStepInfo().args[1];

	int thresh = stoi(Tr.getCurrentScriptStepInfo().args[2]);

	cout << "Beginning find and follow worms" << endl;


	// Store the fluo phenotype of checked worm
	vector<int> fluo_phenotype;


	for (int worms_tracked = 0; worms_tracked < num_worms; worms_tracked++) {
		// Move to a random offset location
		double x_offset = 0;
		double y_offset = 0;

		executeRandomDropOffset(Ox, x_offset, y_offset);
		Ox.goToAbsolute(Tr.getCurrentPosition());
		Ox.goToRelative(Point3d(x_offset, y_offset, 0), 1.0, true, 25);

		worms.image_timer.setToZero();

		boost_sleep_ms(200);

		worms.worm_finding_mode = worms.FIND_ALL;

		// Wait for the worm finder thread to finish finding the worms
		while (worms.worm_finding_mode != worms.WAIT) {
			boost_sleep_ms(50);
			boost_interrupt_point();
		}

		cout << "Worms list size: " << worms.all_worms.size() << endl;

		if (worms.all_worms.size() == 0) return false;

		// Sort the worms based on distance from High Mag FOV center.
		compWormDist compObject; compObject.ref_pt = worms.high_mag_fov_center_loc;
		sort(worms.all_worms.begin(), worms.all_worms.end(), compObject);

		// Track the first worm
		worms.setTrackedWorm(&worms.all_worms[0]);

		Worm& tracked_worm = *(worms.getTrackedWorm());
		worms.worm_finding_mode = worms.TRACK_ONE;

		// Go to worm directly
		double dx = (tracked_worm.getWormCentroidGlobal().x - worms.high_mag_fov_center_loc.x) / Pk.dydOx;
		double dy = (tracked_worm.getWormCentroidGlobal().y - worms.high_mag_fov_center_loc.y) / Pk.dydOx;

		int old_worm_mode = worms.worm_finding_mode;
		worms.worm_finding_mode = worms.WAIT;
		boost_sleep_ms(100);

		cout << "Moving to tracked worm " << endl;
		Ox.goToRelative(dx, dy, 0, 1.0, true, 0);
		worms.updateWormsAfterStageMove(dx, dy, Pk.dydOx);
		//cout << "Targeted worm current pos: " << tracked_worm.getWormCentroidGlobal().x << ", " << tracked_worm.getWormCentroidGlobal().y << endl;

		worms.worm_finding_mode = old_worm_mode;

		// Follow this worm for some time
		//int seconds_to_track = 3;
		AnthonysTimer follow_timer;
		AnthonysTimer grab_segment_image;

		vector<Mat> segment_images;
		vector<string> image_times;

		string time_header = getCurrentTimeString(false, true);

		// Center the worm to high-mag camera
		centerTrackedWormToPoint(worms, Ox, worms.high_mag_fov_center_loc, worms.TRACK_ONE);

		boost_sleep_ms(3000);

		bool worm_selected = centerTrackedWormToPoint(worms, Ox, worms.high_mag_fov_center_loc, worms.TRACK_ONE, true);


		if (!worm_selected) {
			cout << "No worm could be selected in centerworm. Find another worm!" << endl;
			continue;
		}

		// Fluorescence imaging
		ImagingMode(GFP_IMAGING, Grbl2, camFact);
		//boost_sleep_ms(exp + 200);

		Mat fluo_frm;
		worms.img_high_mag.copyTo(fluo_frm);

		boost_sleep_ms(4000);

		// Turn back to brightfield
		ImagingMode(BRIGHTFIELD, Grbl2, camFact);

		// Check whether there is a fluo worm in the high mag
		int num_fluo = worms.countHighMagFluoWorms(fluo_frm, thresh, 0);
		//bool carry_fluo = tracked_worm.segmentFluo(fluo_frm, thresh, 0);
		bool carry_fluo = num_fluo > 0;

		cout << "Carry_fluo = " << carry_fluo << endl;

		if (carry_fluo) {
			fluo_phenotype.push_back(int(1));
		}
		else {
			fluo_phenotype.push_back(int(0));
		}

		cout << "Done following worm " << worms_tracked << endl;
	}

	// Return the type of fluo phenotype of this plate
	cout << vectorSum(fluo_phenotype) << " / " << fluo_phenotype.size() << " are fluo!";

	if (fluo_phenotype.size() == 0) {
		cout << "Fail to phenotype any worm on this plate" << endl;
	}
	else if (double(vectorSum(fluo_phenotype)) > double(fluo_phenotype.size()) * double(0.8))
	{
		cout << "This is a homozygote fluo plate!" << endl;
	}
	else if (double(vectorSum(fluo_phenotype)) <= double(fluo_phenotype.size()) * double(0.8) &&
			double(vectorSum(fluo_phenotype)) > double(fluo_phenotype.size()) * double(0.2)) 
	{
		cout << "This is a heterozygote fluo plate!" << endl;
	}
	else if (double(vectorSum(fluo_phenotype)) <= double(fluo_phenotype.size()) * double(0.2))
	{
		cout << "This is a non-fluo plate!" << endl;
	}

	return true;
}



bool executeTrayCalibration(PlateTray &Tr) {

	
	Tr.generateCoordsBasedOnCalibrationCoords();
	return true;

}


bool executeSleepAssay(WormPick& Pk, PlateTray& Tr, OxCnc& Ox, OxCnc& Grbl2, StrictLock<OxCnc>& oxLock, StrictLock<OxCnc>& grbl2Lock, WormFinder &worms, CameraFactory& camFact, WormSocket* sock_ptr) {

	// Record the important data of the experiment as it runs
	string datetime_string = getCurrentTimeString(false, true);

	// Create folder for bright field frame
	string bright_field_folder = "C:\\WormPicker\\recordings\\Sleep_brightfield\\";
	bright_field_folder.append(datetime_string); bright_field_folder.append("_");

	//_mkdir(bright_field_folder.c_str());

	// one folder for each trial
	string trial;
	cout << "Enter trial name " << " ..." << endl;
	getline(cin, trial);
	bright_field_folder.append(trial); bright_field_folder.append("\\");
	_mkdir(bright_field_folder.c_str());


	// Get the number of plate to screen
	int num_plate = stoi(Tr.getCurrentScriptStepInfo().args[0]);


	vector<string> save_dirs;
	vector<ofstream> txts;
	// One folder for one strain for each trial
	for (int i = 0; i < num_plate; i++) {
		cout << "Enter name for plate # " << i << " ..." << endl;
		string strain_name;
		getline(cin, strain_name);


		string save_dir(bright_field_folder);
		save_dir.append(strain_name);
		save_dir.append("\\");

		// Append to list of save directory
		save_dirs.push_back(save_dir);
		_mkdir(save_dir.c_str());

		// create text file
		ofstream txt;
		string txt_name(save_dir);
		txt_name.append("worm_coord"); txt_name.append(".txt");
		txt.open(txt_name, std::fstream::app);
		txts.push_back(std::move(txt));

		// write header
		txts[i]<< "Session\t" << "Time\t" << "Worm_Idx\t" << "x\t" << "y\t" << endl;
	}


	// Get the offset
	double offset = stod(Tr.getCurrentScriptStepInfo().args[1]);


	// get the time points we'll check the plate
	set<double> times;
	times.insert(stod(Tr.getCurrentScriptStepInfo().args[2]));
	times.insert(stod(Tr.getCurrentScriptStepInfo().args[3]));
	times.insert(stod(Tr.getCurrentScriptStepInfo().args[4]));
	times.insert(stod(Tr.getCurrentScriptStepInfo().args[5]));
	times.insert(stod(Tr.getCurrentScriptStepInfo().args[6]));
	times.insert(stod(Tr.getCurrentScriptStepInfo().args[7]));
	times.insert(stod(Tr.getCurrentScriptStepInfo().args[8]));
	times.insert(stod(Tr.getCurrentScriptStepInfo().args[9]));

	// dummy rows: assuming all plates were put in row 0
	set<int> rows;
	rows.insert(0);

	// Create a vector of all the plates we can use for singling by checking if they're valid locations
	vector<MatIndex> plate_locs = generatePlateLocations(rows, Tr, Pk);

	// Get the time point from user input
	vector<double> time_points;
	for (double time : times) {
		time_points.push_back(time * 60);
	}

	// Run the experiment!
	PlateTray dummy_tray(Tr);

	// Set up timer
	AnthonysTimer Tt;

	double last_time_point = -99999;

	for (int i = 0; i < time_points.size(); i++) {

		cout << "Start session " << i << "..."<< endl;

		if (last_time_point > time_points[i]) {
			cout << "Session " << i << " was skipped due to incorrect time setting..." << endl;
			for (int k = 0; k < num_plate; k++) {
				txts[k] << "Session " << i << " was skipped due to incorrect time setting..." << endl;
			}
			continue;
		}

		Tt.blockForTime(true, time_points[i], 1000);

		// move to all the plates
		for (int k = 0; k < num_plate; k++) {

			dummy_tray.changeScriptId(plate_locs[k]);
			dummy_tray.updateStepInfo();
			dummy_tray.changeScriptArgs({ "1", "0", "0", "0", "0", "1", "0", "0", "0", "0" }); // set move inputs
			executeMoveToPlate(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);

			// Add a little offset to right for better FOV
			Ox.goToRelative(-offset, 0, 0, 1, true);
			executeAutoFocus(Ox, worms, camFact, Tr);

			// Find worms
			executeFindWorm(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact, sock_ptr);

			// Get the worm result
			double this_time = Tt.getElapsedTime();

			if (worms.all_worms.empty()) {
				txts[k]
					<< i << "\t"
					<< this_time << "\t"
					<< -1 << "\t"
					<< -1 << "\t"
					<< -1 << "\t"
					<< endl;
			}
			else {
				for (int j = 0; j < worms.all_worms.size(); j++) {
					txts[k]
						<< i << "\t"
						<< this_time << "\t"
						<< j << "\t"
						<< worms.all_worms[j].getWormCentroidGlobal().x << "\t"
						<< worms.all_worms[j].getWormCentroidGlobal().y << "\t"
						<< endl;
				}
			}


			// Save the bright field frame to disk
			vector<int> params{ CV_IMWRITE_PNG_COMPRESSION, 0 };
			string BR_img_name = save_dirs[k];
			BR_img_name.append("session_"); BR_img_name.append(to_string(i));
			BR_img_name.append(".png");
			imwrite(BR_img_name, worms.imggray, params);

			// Keep tracking of last time point
			last_time_point = Tt.getElapsedTime();

		}

		cout << "End session " << i << "..." << endl;

	}


	// Close all ofstreams
	for (int k = 0; k < num_plate; k++) {
		txts[k].close();
	}

	cout << "Sleep Assay is done..." << endl;

	//TODO: Tune the waiting time and threshold to proper value 1.6 and 0.002

	exit(EXIT_SUCCESS);

	return true;
}


// Test script that examine the time latency between sending command and hardware readiness while switching imaging mode
bool executeMeasureImgLatency(const PlateTray &Tr, OxCnc &Grbl2, CameraFactory& camFact, WormFinder& worms) {

	// index of imaging mode
	int idx_img_mode = stoi(Tr.getCurrentScriptStepInfo().args[0]);

	idx_img_mode = 3;

	// Time duration for frame recording
	double record_time = stod(Tr.getCurrentScriptStepInfo().args[0]);

	record_time = 2;

	// holder for frames and times
	vector<Mat> frames;
	vector<double> times;


	//Start the timer
	AnthonysTimer At;

	// Switch imaging mode
	ImagingMode(imaging_mode(idx_img_mode), Grbl2, camFact);

	// Record
	while (At.getElapsedTime() < record_time)
	{
		frames.push_back(worms.img_high_mag);
		times.push_back(At.getElapsedTime());
		boost_sleep_ms(50);
	}

	// Measure the mean intensity of small ROI at the highmag FOV
	for (int i = 0; i < frames.size(); i++) {
		//Rect ROI(worms.high_mag_fov_center_loc.x - 20, worms.high_mag_fov_center_loc.y - 20, 40, 40);

		double minVal;
		double maxVal;
		Point minLoc;
		Point maxLoc;

		minMaxLoc(frames[i], &minVal, &maxVal, &minLoc, &maxLoc);

		cout << "mean intensity = " << maxVal << ", at time = " << times[i] << endl;
	}

	// Turn back to BF
	ImagingMode(BRIGHTFIELD, Grbl2, camFact);

	return true;
}



// Test script that perform fluorescence images offline analysis acquired from mapping experiment
bool executeGeneticMappingAnalysis(WormFinder& worms) {

	
	// Load images from folder
	string parent_folder = "C:\\WormPicker\\recordings\\Phenotype_images\\2022-03-09\\VC170XLX811_screen_F2_test01\\";
	string BF_folder(parent_folder);
	string GFP_folder(parent_folder);
	string RFP_folder(parent_folder);


	BF_folder.append("Brightfield\\");
	GFP_folder.append("GFP\\");
	RFP_folder.append("RFP\\");


	DirList BF_D(BF_folder, false, "png", "");


	// Output file
	filesys::path ppathObj(parent_folder);

	string tFile = "C:\\WormPicker\\recordings\\Experiments\\";
	string date_string = getCurrentDateString();
	string exp_title = "postprocess_";
	exp_title.append(ppathObj.parent_path().filename().string());

	tFile.append(exp_title); tFile.append("_done_at_");
	tFile.append(date_string); tFile.append(".txt");
	experiment_data.open(tFile, std::fstream::trunc);


	// Phenotypes wanted for analysis e.g. filter out small worms
	vector<sex_types> sex_wanted = { HERM, MALE };
	vector<stage_types> stage_wanted = { ADULT, L4_LARVA };
	vector<morph_types> morph_wanted = { NORMAL, DUMPY};
	

	// Counters
	int num_worm_count = 0; // # of worms pass the filtering
	int num_non_GFP = 0;
	int num_RFP_gv_non_GFP = 0;
	double p_RFP_gv_non_GFP = -1;

	for (int i = 0; i < BF_D.full_files.size(); i++) {


		cout << "Analyzing frame # " << i << " out of " << BF_D.full_files.size() << " in total------------"<< endl;

		// Maybe need to find correct fluore frames associated w/ the brightfield frame
		string BF_f_img = BF_D.full_files[i];

		// find correct fluore frames associated w/ the brightfield
		string GFP_f_img(GFP_folder);
		GFP_f_img.append(regex_replace(BF_D.head_files[i], regex("brightfield"), "GFP"));

		string RFP_f_img(RFP_folder);
		RFP_f_img.append(regex_replace(BF_D.head_files[i], regex("brightfield"), "RFP"));

		cout << "BF_f_img: " << BF_f_img << endl;
		cout << "GFP_f_img: " << GFP_f_img << endl;
		cout << "RFP_f_img: " << RFP_f_img << endl;

		// Read frames
		Mat BF_img = imread(BF_f_img);
		Mat GFP_img = imread(GFP_f_img);
		Mat RFP_img = imread(RFP_f_img);

		cv::cvtColor(BF_img, BF_img, COLOR_RGB2GRAY);
		cv::cvtColor(GFP_img, GFP_img, COLOR_RGB2GRAY);
		cv::cvtColor(RFP_img, RFP_img, COLOR_RGB2GRAY);


		// Start analysis
		cout << "Sending image to Python server" << endl;
		maskRCNN_results results = maskRcnnRemoteInference(BF_img);
		cout << "Results received. " << endl;

		Worm test_worm;

		for (int j = 0; j < results.num_worms; j++) {

			bool negative_found = false;

			test_worm.PhenotypeWormFromMaskRCNNResults(BF_img, results, j, 30, 0.45, false, &GFP_img, &RFP_img, (results.num_worms == 1));

			
			// Count statistics
			if (
				vectorFind(sex_wanted, test_worm.current_phenotype.SEX_type) != -1 &&
				vectorFind(stage_wanted, test_worm.current_phenotype.STAGE_type) != -1 &&
				vectorFind(morph_wanted, test_worm.current_phenotype.MORPH_type) != -1 &&
				test_worm.current_phenotype.GFP_type != UNKNOWN_GFP &&
				test_worm.current_phenotype.RFP_type != UNKNOWN_RFP
			)
			{
				num_worm_count++;

				if (test_worm.current_phenotype.GFP_type == NON_GFP) {

					negative_found = true;
					num_non_GFP++;

					if (test_worm.current_phenotype.RFP_type == RFP) { 
						num_RFP_gv_non_GFP++; 
						negative_found = false;
					}
				}
			}


			// Write to output file
			string msg_negative_found = "";
			if (negative_found) msg_negative_found = "\tnegative";

			test_worm.current_phenotype.WriteToFile(experiment_data, BF_D.head_files[i] + msg_negative_found);
		}

	}

	p_RFP_gv_non_GFP = double(num_RFP_gv_non_GFP) / double(num_non_GFP);

	cout << "# non GFP = " << num_non_GFP << endl;
	cout << "# RFP given non GFP = " << num_RFP_gv_non_GFP << endl;
	cout << "p_RFP_gv_non_GFP = " << p_RFP_gv_non_GFP << endl;

	experiment_data.close();

	controlledExit(false);

	return true;
}


bool executeGenomicIntegFluoAnalysis(WormFinder& worms) {
	// Load images from folder
	string parent_folder = "C:\\Users\\Yuchi\\Box Sync\\Raw data for YX293 Integration\\recordings_YX293_Integration\\Phenotype_images\\2022-05-06\\screen_non_integ_03\\";
	string BF_folder(parent_folder);
	string GFP_folder(parent_folder);


	BF_folder.append("Brightfield\\");
	GFP_folder.append("GFP\\");

	DirList BF_D(BF_folder, false, "png", "");


	// Output file
	filesys::path ppathObj(parent_folder);

	string tFile = "C:\\Users\\Yuchi\\Box Sync\\Raw data for YX293 Integration\\recordings_YX293_Integration\\Experiments\\";
	string date_string = getCurrentDateString();
	string exp_title = "postprocess_";
	exp_title.append(ppathObj.parent_path().filename().string());

	tFile.append(exp_title); tFile.append("_done_at_");
	tFile.append(date_string); tFile.append(".txt");
	experiment_data.open(tFile, std::fstream::trunc);

	// Phenotypes wanted for analysis e.g. filter out small worms
	vector<sex_types> sex_wanted = { HERM, MALE };
	vector<stage_types> stage_wanted = { ADULT, L4_LARVA };
	vector<morph_types> morph_wanted = { NORMAL, DUMPY };

	// Counters
	int num_worm_count = 0; // # of worms pass the filtering

	for (int i = 0; i < BF_D.full_files.size(); i++) {

		cout << "Analyzing frame # " << i << " out of " << BF_D.full_files.size() << " in total------------" << endl;

		// Maybe need to find correct fluore frames associated w/ the brightfield frame
		string BF_f_img = BF_D.full_files[i];

		// find correct fluore frames associated w/ the brightfield
		string GFP_f_img(GFP_folder);
		GFP_f_img.append(regex_replace(BF_D.head_files[i], regex("brightfield"), "GFP"));
	
		cout << "BF_f_img: " << BF_f_img << endl;
		cout << "GFP_f_img: " << GFP_f_img << endl;
	
		// Read frames
		Mat BF_img = imread(BF_f_img);
		Mat GFP_img = imread(GFP_f_img);

		cv::cvtColor(BF_img, BF_img, COLOR_RGB2GRAY);
		cv::cvtColor(GFP_img, GFP_img, COLOR_RGB2GRAY);


		// Start analysis
		cout << "Sending image to Python server" << endl;
		maskRCNN_results results = maskRcnnRemoteInference(BF_img);
		cout << "Results received. " << endl;


		Worm test_worm;


		for (int j = 0; j < results.num_worms; j++) {


			test_worm.PhenotypeWormFromMaskRCNNResults(BF_img, results, j, 30, 0.45, false, &GFP_img, nullptr, (results.num_worms == 1));
		
			if (
				vectorFind(sex_wanted, test_worm.current_phenotype.SEX_type) != -1 &&
				vectorFind(stage_wanted, test_worm.current_phenotype.STAGE_type) != -1 &&
				vectorFind(morph_wanted, test_worm.current_phenotype.MORPH_type) != -1 &&
				test_worm.current_phenotype.GFP_type != UNKNOWN_GFP
				)
			{
				
				num_worm_count++;

				cout << "Max GFP intensity of Worm #" << num_worm_count << " = " << test_worm.current_phenotype.max_GFP_inten << endl;

				test_worm.current_phenotype.WriteToFile(experiment_data, BF_D.head_files[i]);
			}


		
		}
	
	
	
	}

	experiment_data.close();

	exit(EXIT_SUCCESS);


	return true;
}


bool executeFindWorMotelWell(PlateTray &Tr) {

	std::string WM_pattern_path = "C:/Users/Yuchi/Dropbox/UPenn_2022_Fall/WormPicker/Experiment_data/20221027_WM_pattern_match/Test_imgs/*.jpg";
	std::string match_result_path = "C:/Users/Yuchi/Dropbox/UPenn_2022_Fall/WormPicker/Experiment_data/20221027_WM_pattern_match/Results/";

	Tr.WrMtl.getMoatCrossTemplate();
	Tr.WrMtl.DetectMoatCrossCoord(WM_pattern_path, match_result_path, cv::TM_SQDIFF_NORMED);
	return true;

}


// High level test script to run an full experiment that will sort one plate of fluo and non-fluo worms into two plates
// of all fluo and all non-fluo worms.
bool executeSortFluoWorms(WormPick &Pk, PlateTray &Tr, OxCnc &Ox, OxCnc &Grbl2,
	StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock,
	WormFinder& worms, CameraFactory& camFact, int& kg) {
//
//	// args[0]: total amount of worms to sort out
//	int num_worms_to_sort = Tr.getCurrentScriptStepInfo().args[0];
//
//	// Source plate coordinate
//	int source_row = Tr.getCurrentScriptStepInfo().id.row;
//	int source_col = Tr.getCurrentScriptStepInfo().id.col;
//
//	// Non-fluo plate coordinate
//	//int nonfluo_row = Tr.getCurrentScriptStepInfo().args[2];
//	//int nonfluo_col = Tr.getCurrentScriptStepInfo().args[3];
//
//	int herm_row = Tr.getCurrentScriptStepInfo().args[2];
//	int herm_col = Tr.getCurrentScriptStepInfo().args[3];
//
//	// Fluo plate coordinate
//	//int fluo_row = Tr.getCurrentScriptStepInfo().args[4];
//	//int fluo_col = Tr.getCurrentScriptStepInfo().args[5];
//
//	int male_row = Tr.getCurrentScriptStepInfo().args[4];
//	int male_col = Tr.getCurrentScriptStepInfo().args[5];
//
//	// Now go to each plate in the experiment, remove its lid, and drop it one row up for safekeeping.
//	/*vector<tuple<MatIndex, MatIndex>> lift_and_drop_locs;
//	lift_and_drop_locs.push_back(tuple<MatIndex,MatIndex>(MatIndex(source_row, source_col), MatIndex(source_row + 1, source_col)));
//	lift_and_drop_locs.push_back(tuple<MatIndex, MatIndex>(MatIndex(herm_row, herm_col), MatIndex(herm_row + 1, herm_col)));
//	lift_and_drop_locs.push_back(tuple<MatIndex, MatIndex>(MatIndex(male_row, male_col), MatIndex(male_row + 1, male_col)));
//	executeLiftAndDropLids(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, lift_and_drop_locs);*/
//
//
//	int num_sorted_worms = 0;
//
//	MatIndex pickid = Tr.getCurrentScriptStepInfo().id;
//	PlateTray dummy_tray(Tr);
//
//	string tFile = "C:\\WormPicker\\Timing\\Timing_";
//	string date_string = getCurrentDateString();
//
//	tFile.append(date_string); tFile.append(".txt");
//
//	ofstream time_data;
//	time_data.open(tFile, std::fstream::app);
//	AnthonysTimer step_timer;
//	AnthonysTimer total_timer;
//
//	string datetime_string = getCurrentTimeString(false, true);
//
//	time_data << "Experiment started: " << datetime_string;
//
//
//	// Put the pick at the start position
//	//Grbl2.goToAbsolute(Grbl2.getX(), Grbl2.getY(), Pk.getPickupHeight() + Pk.pick_focus_height_offset, true, 0);
//	Pk.movePickToStartPos(Grbl2, 1, 0, 0, 0);
//
//
//	while (num_sorted_worms < num_worms_to_sort) {
//
//		// Set the source plate id
//		pickid = MatIndex(source_row, source_col);
//		dummy_tray.changeScriptId(pickid);
//		dummy_tray.updateStepInfo();
//
//		step_timer.setToZero();
//		total_timer.setToZero();
//		time_data << endl;
//
//		// Go to the source plate
//		dummy_tray.changeScriptArgs(MOVE_TO_SOURCE_DEFAULT); // set move inputs
//		executeMoveToPlate(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);
//
//		double step_time = step_timer.getElapsedTime();
//		double total_time = total_timer.getElapsedTime();
//		cout << "	Move to source: " << step_time << endl;
//		time_data << "MoveToPlate\t" << step_time << "\t" << total_time << "\t";
//		step_timer.setToZero();
//
//		//// Sterilize the pick
//		//dummy_tray.changeScriptArgs({ 4, 10, 0.15, 0, 0, 0, 0, 0, 0, 0 }); // set heat pick inputs
//		//executeHeatPick(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock);
//
//		//step_time = step_timer.getElapsedTime();
//		//total_time = total_timer.getElapsedTime();
//		//cout << "	Heat Pick: " << step_time << endl;
//		//time_data << "Heat Pick\t" << step_time << ", " << total_time << "\t";
//		//step_timer.setToZero();
//
//		// Find and center a worm
//		dummy_tray.changeScriptArgs(CENTER_WORMS_DEFAULT); // set centerworm inputs
//		Phenotype worm_returned;
//		executeCenterWorm(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact, kg, worm_returned);
//
//		step_time = step_timer.getElapsedTime();
//		total_time = total_timer.getElapsedTime();
//		cout << "	CenterWorm: " << step_time << endl;
//		time_data << "CenterWorm\t" << step_time << "\t" << total_time << "\t";
//
//		//boost_sleep_ms(5000);
//		step_timer.setToZero();
//
//		// Pick the worm if one was found
//		if (worm_returned.size() != 0) {
//			dummy_tray.changeScriptArgs(PICK_UP_WORMS_DEFAULT); // set pickworm inputs
//			executeSpeedPick(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);
//		}
//		else {
//			cout << "Failed to select a worm during the execute center worm step." << endl;
//			time_data << "Failed to select a worm" << endl << endl;
//			time_data.terminate();
//			return false;
//		}
//
//		step_time = step_timer.getElapsedTime();
//		total_time = total_timer.getElapsedTime();
//		cout << "	SpeedPick-Pick: " << step_time << endl;
//		time_data << "SpeedPick-Pick\t" << step_time << "\t" << total_time << "\t";
//		step_timer.setToZero();
//
//		// Move to the destination plate based on what type of worm was picked.
//		/*if (worm_returned == worms.NON_FLUO_TYPE)
//			pickid = MatIndex(nonfluo_row, nonfluo_col);
//		else if (worm_returned == worms.FLUO_TYPE)
//			pickid = MatIndex(fluo_row, fluo_col);*/
//
//			// Temporarily put the sex sorting destination plate at this place
//			/*if (worm_returned == worms.HERM)
//				pickid = MatIndex(herm_row, herm_col);
//			else if (worm_returned == worms.MALE)
//				pickid = MatIndex(male_row, male_col);*/
//
//		pickid = MatIndex(herm_row, herm_col);
//
//		dummy_tray.changeScriptId(pickid);
//		dummy_tray.updateStepInfo();
//		dummy_tray.changeScriptArgs(MOVE_TO_DESTINATION_DEFAULT); // set move inputs
//		executeMoveToPlate(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);
//
//		step_time = step_timer.getElapsedTime();
//		total_time = total_timer.getElapsedTime();
//		cout << "	MoveToPlate: " << step_time << endl;
//		time_data << "MoveToPlate\t" << step_time << "\t" << total_time << "\t";
//
//		//boost_sleep_ms(5000);
//		step_timer.setToZero();
//
//		// Drop the worm at the destination plate
//		dummy_tray.changeScriptArgs(DROP_OFF_WORMS_DEFAULT); // set pickworm inputs
//		executeSpeedPick(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);
//
//		step_time = step_timer.getElapsedTime();
//		total_time = total_timer.getElapsedTime();
//		cout << "	SpeedPick-Drop: " << step_time << endl;
//		time_data << "SpeedPick-Drop\t" << step_time << "\t" << total_time << "\t";
//
//		num_sorted_worms++;
//		cout << "Worms sorted: " << num_sorted_worms << endl;
//		time_data << "Worm Num: " << num_sorted_worms << "\t";
//		//string type_sorted = worm_returned == 1 ? "Non-Fluo" : "Fluo";
//		string type_sorted = "Must adjust for worm_returned being a vector now."; // <--- TODO: Fix this
//		time_data << "Type: " << type_sorted;
//	}
//	time_data << endl << endl;
//
//	time_data.terminate();
	return true;
}




bool executeMaskRCNN(WormFinder &worms) {


	// Mask RCNN Segmentation method

	Mat img;
	worms.img_high_mag.copyTo(img);
	maskRCNN_results results = maskRcnnRemoteInference(img);
	Mat mask = results.worms_mask;
	cv::threshold(mask, mask, 0, 255, cv::THRESH_BINARY);
	cv::imshow("Mask from python client", mask);
	waitKey(1000);

	return true;
}



/*
	Anthony's test scripts
*/

// 
void testCentralizedSegmentation() {

	// Open video files
	string pvideo = "C:\\Dropbox\\FramesData\\High_Mag_WormSegment\\";
	string pdebug_frames = pvideo + "debug frames\\";
	string fshort = "2020-10-11_test02_high_mag_crop29.0-65.0s.avi";
	string fname = pvideo + fshort;
	string fvidout = pvideo + fshort.replace(fshort.end() - 4, fshort.end(), "_debug.avi");
	
	
	VideoCapture stream0(fname);
	VideoWriter vidout(fvidout,CV_FOURCC('M', 'J', 'P', 'G'),15,Size(612,512),true);
	
	// Vector of out data
	vector<vector<double>> outdata;

	// verify that camera or file was opened
	if (stream0.isOpened()) {
		cout << "Reading from video file:" << fname << endl;
	}
	else {
		cout << "Could not load video file:" << fname << endl;
	}

	int frame_num = 0;
	Worm current_worm;
	for (;;) {
		// Get an image, convert to gray uint8
		Mat img, img_color;
		stream0.read(img);

		// Normalize frame
		normalize(img, img, 0.0, 255, cv::NORM_MINMAX);

		// If no image was grabbed because we are using a video file and reached the end, go back to the beginning, 
		if (img.empty()) {
			stream0.set(CAP_PROP_POS_MSEC, 0);
			stream0.read(img);
			break;
		}


		if (img.type() != CV_8U) {
			cvtColor(img, img, CV_RGB2GRAY);
		}

		// Segment worm
		string fname_intensity_profile = "C:\\Dropbox\\WormPicker (shared data)\\WormIntensityTemplate.csv";
		
		current_worm.segmentWorm(img);

		// Label worm on color version of image
		cvtColor(img, img_color, CV_GRAY2BGR);
		current_worm.drawSegmentation(img_color);


		// Display the image with segmentation
		int k = showDebugImage(img_color,false,"DEBUG",1);

		///////////////////// DEBUG //////////////////////////////////////////////

		// Save the brightfield intensity
		outdata.push_back(current_worm.bf_profile);


		// Save the frame so we can compare frame number to BF profile
		frame_num += 1;
		string frame_out = pdebug_frames + "frame_" + to_string(frame_num) + ".png";
		//imwrite(frame_out, img_color);
		vidout << img_color;

		// Abort on Esc
		if (k > 0)
			break;

 	}

	///////////////// DEBUG - Print intensity profiles //////////////////////
	string fout = fname.replace(fname.end() - 4, fname.end(), ".csv");
	ofstream outfile(fout);

	if (!outfile.is_open()) {
		cout << "Failed to save file - it is not open - close excel" << endl;
		exit(0);
	}


	for (int i = 0; i < outdata.size(); i++) {
		for (int j = 0; j < outdata[i].size(); j++) {
			outfile << outdata[i][j] << ",";
		}
		outfile << endl;
	}

	outfile.close();

}


/*
	Peter's test scripts
*/


// Will find a worm and track them in the high mag camera. Can optionally segment the worm while we track it.
bool executeFindAndFollowWorms(WormPick &Pk, PlateTray &Tr, OxCnc &Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc>& grbl2Lock, WormFinder& worms, CameraFactory& camFact) {
	
	//int num_worms_to_follow = Tr.getCurrentScriptStepInfo().args[0];
	//bool segmentWorm = Tr.getCurrentScriptStepInfo().args[1];


	//std::vector<double> CENTER_WORMS_DEFAULT{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 }; // Default arguments for finding and centering a worm. (Only center worms in the pickable region)
	//std::vector<double> MOVE_TO_SOURCE_DEFAULT{ 1, 1, 0, 1, 1, 0, 0, 0, 0, 0 }; // Default arguments for moving to a source plate. (Go directly to plate, sterilize, and move the camera away from the field of view, manage lids)

	//PlateTray dummy_tray(Tr);

	////Move to plate
	//dummy_tray.changeScriptArgs(MOVE_TO_SOURCE_DEFAULT); // set move inputs
	//executeMoveToPlate(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);

	//// Prepare script for center worms
	//dummy_tray.changeScriptArgs(CENTER_WORMS_DEFAULT);
	//Phenotype worm_returned;
	//int kg = 0;
	//int num_worms = 1;

	//cout << "Beginning find and follow worms" << endl;
	//for (int worms_tracked = 0; worms_tracked < num_worms_to_follow; worms_tracked++) {
	//	
	//	cout << "Start center worm " << worms_tracked << endl;

	//	// Follow this worm for some time
	//	/*int seconds_to_track = 8;
	//	AnthonysTimer follow_timer;
	//	AnthonysTimer grab_segment_image;

	//	vector<Mat> segment_images;
	//	vector<string> image_times;

	//	string time_header = getCurrentTimeString(false, true);*/

	//	// Run the image focuser
	//	//executeFocusImage(Pk, Tr, Ox, oxLock, worms);

	//	//cout << "follow_timer.getElapsedTime() = " << follow_timer.getElapsedTime() << endl;

	//	int num_tries = 0;
	//	int max_tries = 50;
	//	bool success = false;
	//	do {
	//		cout << "Attempting to find and center a worm for picking. Try: " << num_tries + 1 << endl;
	//		//experiment_data << "\tAttempt " << num_tries + 1 << " of " << max_tries << " to find and center a worm for picking." << endl;
	//		dummy_tray.changeScriptArgs(CENTER_WORMS_DEFAULT); // set centerworm inputs
	//		success = executeCenterWorm(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact, kg, worm_returned, num_worms);
	//		num_tries++;
	//		if (!success) {
	//			//experiment_data << "\tFailed to find and center a worm for picking." << endl;
	//		}

	//	} while (!success && num_tries < max_tries);

	//	if (!success) {
	//		cout << "Fail centering worm" << endl;
	//		break;
	//	}

	//	cout << "After centering worm" << endl;

	//	//while (follow_timer.getElapsedTime() < seconds_to_track) {

	//	//	centerTrackedWormToPoint(worms, Ox, worms.high_mag_fov_center_loc, worms.TRACK_ONE, false, false);
	//	//	// boost_sleep_ms(50);


	//	//	// If desired, grab an image to segment every 2 seconds.
	//	//	if (segmentWorm && grab_segment_image.getElapsedTime() > 1) {


	//	//		// Fluo imaging
	//	//		/*int exp = 1000;
	//	//		ImagingMode(1, exp, Grbl2, camFact);
	//	//		boost_sleep_ms(exp + 200);*/

	//	//		/*for (int k = 0; k < 3 * 2; k++) {
	//	//			Mat segment_image;
	//	//			worms.img_high_mag.copyTo(segment_image);
	//	//			segment_images.push_back(segment_image);
	//	//			boost_sleep_ms(exp + 1000);
	//	//		}*/

	//	//		Mat segment_image_fluo;
	//	//		Mat segment_image_BF;

	//	//		// Brightfield imaging
	//	//		ImagingMode(0, 3, Grbl2, camFact);
	//	//		worms.img_high_mag.copyTo(segment_image_BF);
	//	//		segment_images.push_back(segment_image_BF);


	//	//		// Fluo imaging
	//	//		int exp = 1000;
	//	//		ImagingMode(1, exp, Grbl2, camFact);
	//	//		boost_sleep_ms(exp + 200);
	//	//		worms.img_high_mag.copyTo(segment_image_fluo);
	//	//		segment_images.push_back(segment_image_fluo);


	//	//		//image_times.push_back(getCurrentTimeString(false, true));
	//	//		grab_segment_image.setToZero();

	//	//		// Brightfield imaging
	//	//		//ImagingMode(0, 3, Grbl2, camFact);

	//	//		boost_sleep_ms(200);
	//	//	}


	//	//}

	//	//////////// Saving the recorded images to disk
	//	//string file_dir = "C:/WormPicker/Fluorescence_data/20210411/YX261/";
	//	//string file_dir = "C:\\WormPicker\\Genetic_cross_example_frames\\LX831\\";
	//	//file_dir.append(getCurrentDateString()); file_dir.append("_WormData.txt");

	//	//ofstream worm_data;
	//	//if (segmentWorm) {
	//	//	// Write the contour area and the worm length to disk
	//	//	worm_data.open(file_dir, std::fstream::app);
	//	//	worm_data << getCurrentTimeString(false, true);
	//	//}


	//	//Save the images to disk
	//	//vector<int> params{ CV_IMWRITE_PNG_COMPRESSION, 0 }; // Set the paramters for saving frame. E.g. compression level
	//	//for (int i = 1; i < segment_images.size(); i++) {
	//	//	// Save the raw image to disk
	//	//	//string img_dir = "C:/WormPicker/StagingData/TestData/";
	//	//	//file_dir.append(image_times[i]); file_dir.append("_raw.png");
	//	//	string f (file_dir);
	//	//	f.append(time_header); f.append("_"); f.append(to_string(i)); f.append(".png");

	//	//	//cout << f << endl;
	//	//	imwrite(f, segment_images[i], params);

	//	//	//// segment the image
	//	//	//tracked_worm.segmentWorm(segment_images[i]);
	//	//	//cv::cvtColor(segment_images[i], segment_images[i], CV_GRAY2BGR);
	//	//	//tracked_worm.drawSegmentation(segment_images[i]);

	//	//	//// Save the segmented image to disk
	//	//	//img_dir = "C:/WormPicker/StagingData/TestData/";
	//	//	//img_dir.append(image_times[i]); img_dir.append("_segmented.png");
	//	//	//imwrite(img_dir, segment_images[i], params);

	//	//	// Write the worm data to the file
	//	//	/*worm_data << "Contour Area:\t" << tracked_worm.getAreaV2() << "\t";
	//	//	worm_data << "Worm Length (pixels):\t" << tracked_worm.worm_length << endl;*/

	//	//}

	//	//worm_data.terminate();

	//	cout << "Done following worm " << worms_tracked << endl;


	//	cout << "Check exit flag for loop " << (worms_tracked < num_worms_to_follow) << endl;
	//}


	//cout << "Exit following worm script" << endl;

	return true;
}

bool executeTestAutoLidHandler(WormPick& Pk, PlateTray& Tr, OxCnc& Ox, OxCnc& Grbl2, StrictLock<OxCnc>& oxLock, StrictLock<OxCnc>& grbl2Lock, WormFinder& worms, CameraFactory& camFact) {
	// plate coordinates of the first source plate
	int source1_row = Tr.getCurrentScriptStepInfo().id.row;
	int source1_col = Tr.getCurrentScriptStepInfo().id.col;
	MatIndex source1_loc(source1_row, source1_col);

	//args[0], args[1] row and col, respectively, of the second source plate
	int source2_row = stoi(Tr.getCurrentScriptStepInfo().args[0]);
	int source2_col = stoi(Tr.getCurrentScriptStepInfo().args[1]);
	MatIndex source2_loc(source2_row, source2_col);

	//args[2], args[3] row and col, respectively, of the destination plate
	int dest_row = stoi(Tr.getCurrentScriptStepInfo().args[2]);
	int dest_col = stoi(Tr.getCurrentScriptStepInfo().args[3]);
	MatIndex dest_loc(dest_row, dest_col);
	Pk.lid_handler.setPlateType(dest_loc, LidHandler::IS_DESTINATION);

	//args[4], args[5] row and col, respectively, of the intermediate plate
	int int_row = stoi(Tr.getCurrentScriptStepInfo().args[4]);
	int int_col = stoi(Tr.getCurrentScriptStepInfo().args[5]);
	MatIndex int_loc(int_row, int_col);
	Pk.lid_handler.setPlateType(int_loc, LidHandler::IS_DESTINATION);

	vector<MatIndex> plates = { source1_loc, source2_loc, dest_loc, int_loc };
	PlateTray dummy_tray(Tr);
	srand((unsigned)time(NULL));

	// Randomly move to all of the plates to check if the lid handling is working properly
	for (int i = 0; i < 20; i++) {
		// Generate x and y offset
		int plate_idx = rand() % plates.size(); // Generate an number between 0 and 3 (because 4 plates)
		MatIndex move_to_plate = plates[plate_idx];

		cout << "Attempt: " << i << ", idx: " << plate_idx << ", (" << move_to_plate.row << ", " << move_to_plate.col << ")" << endl;

		dummy_tray.changeScriptId(move_to_plate);
		dummy_tray.updateStepInfo();
		dummy_tray.changeScriptArgs({ "1", "0", "0", "0", "1", "0", "0", "0", "0", "0" }); // set move inputs

		executeMoveToPlate(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);

	}

	return true;
}

bool executeTestPhenotyping(WormPick &Pk, PlateTray &Tr, OxCnc &Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc>& grbl2Lock, WormFinder& worms, CameraFactory& camFact) {
	//// Ask user for cross parameters (what phenotyping they would like to do)
	//vector<phenotyping_method> cross_parameters = getWantedPhenotypeFromInput(1);

	//PlateTray dummy_tray(Tr);

	//// Get the number of worms to pick from each source plate
	//int source1_num = Tr.getCurrentScriptStepInfo().args[0];

	//// plate coordinates of the first source plate
	//int source1_row = Tr.getCurrentScriptStepInfo().id.row;
	//int source1_col = Tr.getCurrentScriptStepInfo().id.col;
	//MatIndex source1_loc(source1_row, source1_col);

	//int kg = 0;
	//int num_worms = 0;

	//dummy_tray.changeScriptId(source1_loc);
	//dummy_tray.updateStepInfo();
	//current_phenotyping_method = cross_parameters[1];

	//for (int i = 0; i < source1_num; i++) {
	//	cout << "Attempting to find, pick, and drop worm: " << i << endl;

	//	cout << "============= Moving to source ==============" << endl;
	//	dummy_tray.changeScriptArgs({ 1, 1, 0, 1, 1, 0, 0, 0, 0, 0 }); // set move inputs
	//	executeMoveToPlate(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);

	//	// Find and center a worm to pick
	//	cout << "============= Finding and centering a worm ==============" << endl;
	//	Phenotype worm_returned;
	//	int num_tries = 0;
	//	int max_tries = 27;
	//	dummy_tray.changeScriptArgs({ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 }); // set centerworm inputs
	//	bool success = false;
	//	do {
	//		cout << "Attempting to find and center a worm for picking. Try: " << num_tries + 1 << endl;
	//		success = executeCenterWorm(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact, kg, worm_returned, num_worms);
	//		num_tries++;

	//	} while (!success && num_tries < max_tries);

	//	imshow("high mag worm img", worms.img_high_mag_color);
	//	waitKey(0);
	//	destroyWindow("high mag worm img");
	//}

	//Pk.lid_handler.dropAllLids(Tr, Ox);
	
	return true;
}



bool executeTestAutoFocus(WormPick &Pk, PlateTray &Tr, OxCnc &Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc>& grbl2Lock, WormFinder& worms, CameraFactory& camFact) {

	int test1_row = Tr.getCurrentScriptStepInfo().id.row;
	int test1_col = Tr.getCurrentScriptStepInfo().id.col;
	MatIndex test1_loc(test1_row, test1_col);

	int offset = stoi(Tr.getCurrentScriptStepInfo().args[0]);

	PlateTray dummy_tray(Tr);
	dummy_tray.changeScriptId(test1_loc);
	dummy_tray.updateStepInfo();
	dummy_tray.changeScriptArgs({ "1", "0", "0", "0", "0", "0", "0", "0", "0", "0" }); // set move inputs
	Rect custom_region(-3, -20, 6, 8);
	move_offset_region = &custom_region;
	executeMoveToPlate(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);

	executeAutoFocus(Ox, worms, camFact, Tr);

	return true;
	

	// Enable fluorescence imaging
	// Need to set low mag camera to long exposure time inside imaging mode
	ImagingMode(LASER_AUTOFOCUS, Grbl2, camFact);

	boost_sleep_ms(1000);

	// ROI for laser tracking.
	Size low_mag_size = worms.imggray.size();
	int width = low_mag_size.width * worms.laser_focus_roi_w_mult;
	int height = low_mag_size.height * worms.laser_focus_roi_h_mult;
	cout << "laser focus rect width, height: " << width << ", " << height << endl;
	Rect low_mag_laser_rect = Rect(low_mag_size.width / 2 - width / 2, low_mag_size.height / 2 - height / 2, width, height);

	shared_ptr<AbstractCamera> low_mag_cam = camFact.firstCamera();
	shared_ptr<StrictLock<AbstractCamera>> mainCamLock = camFact.getCameraLock(low_mag_cam);
	Mat wait_mat;
	cout << "Pre change exposure: " << endl;
	low_mag_cam->getExposureTime(*mainCamLock);

	//for (int i = -1; i > -16; i--) {
	//	worms.imggray.copyTo(wait_mat);
	//	Mat laser_roi(wait_mat(low_mag_laser_rect));
	//	imshow("Laser roi", laser_roi);
	//	waitKey(0);
	//	cout << "waitkey pressed" << endl;
	//	mainCamLock->lock();
	//	low_mag_cam->setExposureTime(*mainCamLock, i);
	//	mainCamLock->unlock();
	//	cout << "Exposure: " << i << endl;
	//	cout << endl;
	//	//boost_sleep_ms(5000);
	//}


	AnthonysTimer focus_timer;
	AnthonysTimer overall_timer;
	int max_col = 0;
	bool focused = false;
	worms.laser_focus_display = true; // to display the ROI for laser tracking. Note as of now the ROI is hard coded to display, so if you change it here you must also change it in WPHelper
	int last_framenum = WP_main_loop_framenum;
	while (!focused) {
		focus_timer.setToZero();
		// Get low mag cropped image ( we are only interested in the small section around the middle with the laser in it)
		// In order to be able to jog the Z direction as quickly as possible, we will grab the image directly from the camera
		// rather than use the imggray in Wormfinder (which is only updated once every main loop).
		/*mainCamLock->lock();
		low_mag_cam->grabImage(*mainCamLock);
		Mat full_img = low_mag_cam->getCurrentImgGray(*mainCamLock);
		mainCamLock->unlock();
		Mat low_mag_laser_roi(full_img(low_mag_laser_rect));*/
		Mat low_mag_laser_roi(worms.imggray(low_mag_laser_rect));

		// Get the intensity profile of the laser in x direction
		max_col = getCliffOfIntensityDistribution(low_mag_laser_roi) + (low_mag_size.width / 2 - width / 2);
		int dist_to_focus = max_col - (worms.high_mag_fov_center_loc.x + offset); // From testing optimal offset ~5... but we can still add additional offset from script args if we want

		cout << "Time to get laser line loc: " << endl;
		focus_timer.printElapsedTime();
		cout << "Col with max avg intensity: " << max_col << endl;
		cout << "Distance to focus: " << dist_to_focus << endl;

		if (abs(dist_to_focus) <= 1){
			focused = true;
		}
		else {
			int direction = dist_to_focus > 0 ? 1 : -1;
			Ox.jogOx(0, 0, direction * 0.05);
			//Ox.goToRelative(0, 0, direction * 0.05, 1.0, true);
		}
		// Synchronize this loop with the framerate of the main loop because the image we use to find the laser is only updated in the main loop
		int count = 0;
		while (last_framenum == WP_main_loop_framenum) {
			boost_sleep_ms(10);
			count++;
		}
		last_framenum = WP_main_loop_framenum;
		cout << "Waited for main loop: " << count << endl;
		
	}

	// Display results
	Mat debug_img;
	worms.imggray.copyTo(debug_img);
	cvtColor(worms.imggray, debug_img, CV_GRAY2RGB);
	rectangle(debug_img, low_mag_laser_rect, colors::yy, 2);
	Point pt1 = Point(max_col, 0);
	Point pt2 = Point(max_col, debug_img.size().height - 1);
	Point pt1_left = Point(max_col-10, 0);
	Point pt2_left = Point(max_col-10, debug_img.size().height - 1);
	Point pt1_right = Point(max_col+10, 0);
	Point pt2_right = Point(max_col+10, debug_img.size().height - 1);
	line(debug_img, pt1, pt2, colors::gg, 1);
	line(debug_img, pt1_left, pt2_left, colors::pp, 1);
	line(debug_img, pt1_right, pt2_right, colors::pp, 1);
	resize(debug_img, debug_img, Size(1200, 800));
	imshow("Laser line", debug_img);
	waitKey(0);

	// Return to brightfield imaging mode
	cout << "Overall time to focus: " << endl;
	overall_timer.printElapsedTime();
	boost_sleep_ms(3000);
	ImagingMode(BRIGHTFIELD, Grbl2, camFact);
	worms.laser_focus_display = false;

	return true;
}

bool executeTestBlueLightStimulus(WormPick &Pk, PlateTray &Tr, OxCnc &Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc>& grbl2Lock, WormFinder& worms, CameraFactory& camFact) {
	vector<int> test_vect = { 1,2,3,4 };
	Scalar average_test_vect = sum(test_vect);
	cout << "test vec scalar: " << average_test_vect << endl;
	
	int test1_row = Tr.getCurrentScriptStepInfo().id.row;
	int test1_col = Tr.getCurrentScriptStepInfo().id.col;
	MatIndex test1_loc(test1_row, test1_col);

	int blue_light_duration = stoi(Tr.getCurrentScriptStepInfo().args[0]);
	int record_activity_duration = stoi(Tr.getCurrentScriptStepInfo().args[1]);
	int secs_between_images = stoi(Tr.getCurrentScriptStepInfo().args[2]);

	PlateTray dummy_tray(Tr);
	dummy_tray.changeScriptId(test1_loc);
	dummy_tray.updateStepInfo();
	dummy_tray.changeScriptArgs({ "1", "0", "0", "0", "1", "1", "0", "0", "0", "0" }); // set move inputs
	executeMoveToPlate(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);

	// Open a file to record the results
	ofstream activity_data;
	string tFile = "C:\\WormPicker\\recordings\\Experiments\\";
	string date_string = getCurrentDateString();
	string datetime_string = getCurrentTimeString(false, true);
	string exp_title = "activity_data";
	tFile.append(exp_title); tFile.append("_");
	tFile.append(date_string); tFile.append(".txt");
	activity_data.open(tFile, std::fstream::app);
	activity_data << "======================\nTest blue light stimulus on worm activity" << endl;
	activity_data << "Time for recording activity before and after blue light (seconds): " << record_activity_duration << endl;
	activity_data << "Blue light duration (seconds): " << blue_light_duration << endl;
	activity_data << "Time to wait between before and after activity images (seconds): " << secs_between_images << endl << endl;;
	
	// Record the activity of the worm population for a time
	AnthonysTimer activity_timer;
	Mat before_img;
	Mat after_img;
	Mat pixel_change;
	Mat pixel_change_thresh;

	Mat before_disp;
	Mat after_disp;
	Mat pix_disp;
	Mat pix_thresh_disp;

	vector<vector<int>> activities(2);
	vector<vector<int>> activities_thresh(2);
	cout << "Recording activity before blue light stimulus." << endl;
	for (int i = 0; i < 2; i++) {
		while (activity_timer.getElapsedTime() < record_activity_duration) {
			cout << endl;
			worms.imggray.copyTo(before_img);
			boost_sleep_ms(secs_between_images * 1000); // Wait seconds between frames;
			worms.imggray.copyTo(after_img);

			pixel_change = after_img - before_img;
			double activity_sum = cv::sum(pixel_change)[0] / 255;
			activities[i].push_back(activity_sum);

			threshold(pixel_change, pixel_change_thresh, 30, 255, THRESH_BINARY);
			double activity_sum_thresh = cv::sum(pixel_change_thresh)[0] / 255;
			activities_thresh[i].push_back(activity_sum_thresh);

			cout << "Activity: " << activity_sum << endl;
			cout << "Activity thresh: " << activity_sum_thresh << endl;

			/*resize(before_img, before_disp, Size(900, 600));
			resize(after_img, after_disp, Size(900, 600));
			resize(pixel_change, pix_disp, Size(900, 600));
			resize(pixel_change_thresh, pix_thresh_disp, Size(900, 600));

			imshow("before", before_disp);
			imshow("after", after_disp);
			imshow("pixel change", pix_disp);
			imshow("pixel change thresh", pix_thresh_disp);
			waitKey(0);
			destroyWindow("before");
			destroyWindow("after");
			destroyWindow("pixel change");
			destroyWindow("pixel change thresh");*/

		}
		// Stimulate the worms with blue light and then record their activity again.
		if (i == 0) {
			stimulateMovementWithBlueLight(worms, Grbl2, camFact, blue_light_duration);
			boost_sleep_ms(2000);
			activity_timer.setToZero();
			cout << "Recording activity after blue light stimulus." << endl;
		}
	}

	cout << "Activity before stim (pure, thresholded): " << endl;
	activity_data << "Activity before stim (pure, thresholded): " << endl;
	for (int i = 0; i < activities[0].size(); i++) {
		cout << "\t" << activities[0][i] << ", " << activities_thresh[0][i] << endl;
		activity_data << "\t" << activities[0][i] << ", " << activities_thresh[0][i] << endl;
	}

	cout << "Activity after stim (pure, thresholded): " << endl;
	activity_data << "Activity after stim (pure, thresholded): " << endl;
	for (int i = 0; i < activities[1].size(); i++) {
		cout << "\t" << activities[1][i] << ", " << activities_thresh[1][i] << endl;
		activity_data << "\t" << activities[1][i] << ", " << activities_thresh[1][i] << endl;
	}

	double average_act_before = ((double)sum(activities[0])[0])/activities[0].size();
	double average_act_thresh_before = ((double)sum(activities_thresh[0])[0]) / activities_thresh[0].size();

	double average_act_after = ((double)sum(activities[1])[0]) / activities[1].size();
	double average_act_thresh_after = ((double)sum(activities_thresh[1])[0]) / activities_thresh[1].size();

	cout << endl;
	cout << "Average activity before stimulus: " << average_act_before << endl;
	cout << "Average thresholded activity before stimulus: " << average_act_thresh_before << endl;
	cout << "Average activity after stimulus: " << average_act_after << endl;
	cout << "Average thresholded activity after stimulus: " << average_act_thresh_after << endl;

	activity_data << endl;
	activity_data << "Average activity before stimulus: " << average_act_before << endl;
	activity_data << "Average thresholded activity before stimulus: " << average_act_thresh_before << endl;
	activity_data << "Average activity after stimulus: " << average_act_after << endl;
	activity_data << "Average thresholded activity after stimulus: " << average_act_thresh_after << endl;

	Pk.lid_handler.dropAllLids(Tr, Ox);
	cout << "Experiment finished!" << endl;

	return true;
}



// args[0]: 1 if we assume the robot is in place to read bar code; 0 otherwise

bool executeTestReadBarcode(WormPick &Pk, PlateTray &Tr, OxCnc &Ox, OxCnc &Grbl2,
	StrictLock<OxCnc> &oxLock, StrictLock<OxCnc>& grbl2Lock, WormFinder& worms,
	CameraFactory& camFact, PlateTracker& plateTracker) {

	// Read from the user script input
	bool is_inplace = stoi(Tr.getCurrentScriptStepInfo().args[0]) != 0;

	//TODO: if NOT in place, we move camera
	// executeMoveToPlate(Pk, Tr);

	// Start to read
	// Turn off bright field illumination, turn on side illumination
	ImagingMode(BARCODE_READING, Grbl2, camFact);
	
	// Get the pointer to access the barcode reading camera
	shared_ptr<AbstractCamera> BarcodeCam = camFact.Nth_camera(1);
	shared_ptr<StrictLock<AbstractCamera>> BarcodeCamLock = camFact.getCameraLock(BarcodeCam);

	// Grab image from the bar code reading camera
	Mat barcode_img;
	BarcodeCam->grabImage(*BarcodeCamLock);
	BarcodeCam->getCurrentImgGray(*BarcodeCamLock).copyTo(barcode_img);
	
	// Display the image
	//Mat barcode_img_display;
	//resize(barcode_img, barcode_img_display, BarcodeCam->getGraySize());
	//imshow("barcode",barcode_img_display);
	//waitKey(2000);

	// Revert back to brightfield
	ImagingMode(BRIGHTFIELD, Grbl2, camFact);

	// Assign barcode image to plate tracker
	plateTracker.setBarcodeImage(barcode_img);

	// Finish upon success
	return true;
}


