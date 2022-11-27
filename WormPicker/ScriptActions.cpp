/*
	ScriptActions.cpp
	Anthony Fouad
	Fang-Yen Lab
	December 2018

	Library of low-level actions that can be requested by WormScript.

	executeScriptAction interperets the action from the plate tray's script object, likely from the script step,
	and passes along references to the hardware needed to complete the action to the corresponding
	action function.
*/
#pragma once
// Anthony's includes
#include "ScriptActions.h"
#include "Script.h"
#include "OxCnc.h"
#include "ErrorNotifier.h"
#include "ImageFuncs.h"
#include "WPHelper.h"
#include "ControlPanel.h"
#include <fstream>
#include "Worm.h"
#include "TestScripts.h" // Can be excluded later - just using for a test
#include "AnthonysCalculations.h"
#include "mask_rcnn.h"
#include <set>
#include "Config.h"
#include "Database.h"


// Socket programming includes
#include "WormSocket.h"

// Boost includes for socket creation
#include <boost/asio.hpp>

//#include "AnthonysCalculations.h"

// OpenCV includes
#include "opencv2\opencv.hpp"

// Namespaces
using namespace std;
using namespace cv;
using json = nlohmann::json;

//std::string fluo_img_dir = "C:\\WormPicker\\Fluorescence_data\\";
std::string phenotype_img_dir = "C:\\WormPicker\\recordings\\Phenotype_images\\";
std::string BF_img_dir;
std::string GFP_img_dir;
std::string RFP_img_dir;

std::string global_exp_title = "";  //Global variable to hold the user entered title of the experiment. This is input in by the user in the main function.
int WP_main_loop_framenum = -1;

// Constructor for phenotyping_method class
phenotyping_method::phenotyping_method(
	bool to_check_sex,
	bool to_check_GFP,
	bool to_check_RFP,
	bool to_check_morphology,
	bool to_check_stage) :

	check_sex(to_check_sex),
	check_GFP(to_check_GFP),
	check_RFP(to_check_RFP),
	check_morphology(to_check_morphology),
	check_stage(to_check_stage)
{
	phenotype_wanted = Phenotype(); // If user set check phenotype this way, then set no phenotype_wanted by default
}

phenotyping_method::phenotyping_method():
	check_sex(false),
	check_GFP(false),
	check_RFP(false),
	check_morphology(false),
	check_stage(false) 
{
	phenotype_wanted = Phenotype();// If user set check phenotype this way, then set no phenotype_wanted by default
}

phenotyping_method::phenotyping_method(Phenotype& ph_wanted) {

	// Set wanted phenotype
	setWantedPhenotype(ph_wanted);

	// Set phenotyping method to be executed to check for the wanted phenotype
	check_sex = (ph_wanted.SEX_type != UNKNOWN_SEX);
	check_GFP = (ph_wanted.GFP_type != UNKNOWN_GFP);
	check_RFP = (ph_wanted.RFP_type != UNKNOWN_RFP);
	check_morphology = (ph_wanted.MORPH_type != UNKNOWN_MORPH);
	check_stage = (ph_wanted.STAGE_type != UNKNOWN_STAGE);
}

void phenotyping_method::setWantedPhenotype(sex_types sex_type, GFP_types gfp_type, RFP_types rfp_type, morph_types morph_type, stage_types stage_type) {
	phenotype_wanted = Phenotype(sex_type, gfp_type, rfp_type, morph_type, stage_type);
}

void phenotyping_method::setWantedPhenotype(Phenotype& Ph) {
	phenotype_wanted = Ph;
}

Phenotype phenotyping_method::getWantedPhenotype() const  {
	return phenotype_wanted;
}

// TODO: delete the deprecated version of the following desired phenotyping options for the current plate
//std::vector<phenotype_options> current_phenotyping_desired; // Holds the desired phenotyping options for the current plate

phenotyping_method phenotyping_method_being_used; // Holds the desired phenotyping options for the current plate

// Default plate tray script arguments for various actions
// If you would like to use options other than the defaults here then you can just build your own vector and pass that instead.
// Check the method you will be calling for descriptions of the script arguments for that method.
std::vector<string> PICK_UP_WORMS_DEFAULT{ "4", "16", "0", "0", "1", "1", "0", "0", "0", "0" }; //14// Default arguments for picking a worm. See SpeedPick for argument descriptions
std::vector<string> DROP_OFF_WORMS_DEFAULT{ "3", "-18", "8", "0", "0", "1", "0", "0", "0", "0"}; // Default arguments for dropping a worm. See SpeedPick for argument descriptions
std::vector<string> DROP_OFF_WORMS_ON_EMPTY_DEFAULT{ "3", "-18", "8", "0", "0", "1", "1", "0", "0", "0"}; // Default arguments for dropping a worm. See SpeedPick for argument descriptions
std::vector<string> MOVE_TO_SOURCE_DEFAULT{ "1", "1", "0", "1", "1", "1", "0", "0", "0", "0" }; // Default arguments for moving to a source plate. (Go directly to plate, sterilize, and move the camera away from the field of view, manage lids, autofocus)
std::vector<string> MOVE_TO_DESTINATION_DEFAULT{ "1", "0", "1", "0", "1", "1", "0", "0", "0", "0" }; // Default arguments for moving to a destination plate. (Go directly to plate, don't sterilize, manage lids, autofocus)
std::vector<string> MOVE_TO_SCREEN_PLATE_DEFAULT{ "1", "0", "0", "1", "1", "1", "0", "0", "0", "0" };// Default arguments for moving to a plate for screen. (Go directly to plate, and move the camera away from the field of view, manage lids, autofocus)
std::vector<string> CENTER_WORMS_DEFAULT{ "1", "0", "0", "0", "0", "0", "0", "0", "0", "0" }; // Default arguments for finding and centering a worm. (Only center worms in the pickable region)

std::ofstream experiment_data;
cv::Rect* move_offset_region = nullptr; // Stores the region in which we will perform a random move offset if desired, when moving around a plate.


bool executeScriptAction(PlateTray& Tr, ErrorNotifier& Err, WormPick &Pk, OxCnc &Ox, OxCnc &Grbl2,
	StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder& worms,
	int& kg, CameraFactory& camFact, PlateTracker& plateTracker, WormSocket* sock_ptr)
{
	cout << Tr.getCurrentScriptStepInfo().step_action << endl;

	/*if (Tr.getCurrentScriptStepInfo().step_action.compare("LiftLid") == 0)
		return executeLiftLid(Pk, Ox, Grbl2, oxLock, grbl2Lock);

	else if (Tr.getCurrentScriptStepInfo().step_action.compare("LowerLid") == 0)
		return executeLowerLid(Pk, Ox, Grbl2, oxLock, grbl2Lock);

	else if (Tr.getCurrentScriptStepInfo().step_action.compare("MoveAndLift") == 0)
		return executeMoveAndLift(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock);*/

	if (Tr.getCurrentScriptStepInfo().step_action.compare("PlateMapping") == 0)
		return executePlateMapping(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);

	else if (Tr.getCurrentScriptStepInfo().step_action.compare("FindWorm") == 0)
		return executeFindWorm(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact, sock_ptr);

	// Below method has been moved to TestScripts.cpp
	/*else if (Tr.getCurrentScriptStepInfo().step_action.compare("MeasrPixDiff") == 0)
		return executeMeasrPixDiff(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, worms);*/

	else if (Tr.getCurrentScriptStepInfo().step_action.compare("PickNWorms") == 0)
		return executePickNWorms(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact, kg, false);


	else if (Tr.getCurrentScriptStepInfo().step_action.compare("SpeedPick") == 0)
		return executeSpeedPick(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);

	else if (Tr.getCurrentScriptStepInfo().step_action.compare("CenterWorm") == 0) {
		vector<Phenotype> dummy_phenotype_result;
		int dummy_num_worm;
		return executeCenterWorm(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact, kg, dummy_phenotype_result, dummy_num_worm);
	}

	// Below method has been moved to TestScripts.cpp
	/*else if (Tr.getCurrentScriptStepInfo().step_action.compare("CenterLarva") == 0)
		return executeCenterLarva(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, worms, kg_or_rflag_Script_thread_isReady);*/

	else if (Tr.getCurrentScriptStepInfo().step_action.compare("MoveToPlate") == 0)
		return executeMoveToPlate(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact, plateTracker);

	else if (Tr.getCurrentScriptStepInfo().step_action.compare("FocusImage") == 0)
		return executeFocusImage(Pk, Tr, Ox, oxLock, worms);

	else if (Tr.getCurrentScriptStepInfo().step_action.compare("HeatPick") == 0)
		return executeHeatPick(Pk, Tr, Grbl2);

	// Below methods have been moved to TestScripts.cpp
	else if (Tr.getCurrentScriptStepInfo().step_action.compare("FluoImg") == 0)
		return executeFluoImg(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, camFact);

	/*
	else if (Tr.getCurrentScriptStepInfo().step_action.compare("CenterFluoWrm") == 0)
		return executeCenterFluoWorm(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);

	else if (Tr.getCurrentScriptStepInfo().step_action.compare("MeasureFluo") == 0)
		return executeMeasureFluo(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);*/

	else if (Tr.getCurrentScriptStepInfo().step_action.compare("PhenotypeFluo") == 0)
		return executePhenotypeFluo(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);

	// Below method has been moved to TestScripts.cpp
	/*else if (Tr.getCurrentScriptStepInfo().step_action.compare("MsrHeadTail") == 0)
		return executeMeasureHeadTail(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, worms);*/

	else if (Tr.getCurrentScriptStepInfo().step_action.compare("PhtyFluoPlt") == 0)
		return executePhtyFluoPlt(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);

	else if (Tr.getCurrentScriptStepInfo().step_action.compare("MoveRel") == 0)
		return executeMoveRel(Tr, Ox, oxLock);

	else if (Tr.getCurrentScriptStepInfo().step_action.compare("CalibCam") == 0)
		return executeCalibrateCam(camFact);

	else if (Tr.getCurrentScriptStepInfo().step_action.compare("CalibOx") == 0)
		return executeCalibrateOxCnc(Pk, Tr, Ox, oxLock, worms, Grbl2, camFact);

	else if (Tr.getCurrentScriptStepInfo().step_action.compare("CalibPick") == 0)
		return executeCalibratePick(Tr, Pk, Grbl2, oxLock, worms);

	else if (Tr.getCurrentScriptStepInfo().step_action.compare("SortFluo") == 0)
		return executeSortFluoWorms(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact, kg);
	
	else if (Tr.getCurrentScriptStepInfo().step_action.compare("CrossStrains") == 0)
		return executeCross(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact, kg);

	else if (Tr.getCurrentScriptStepInfo().step_action.compare("SingleWorms") == 0)
		return executeSingleWorms(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact, kg);

	else if (Tr.getCurrentScriptStepInfo().step_action.compare("ScreenPlates") == 0)
		return executeScreenPlates(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact, kg);

	else if (Tr.getCurrentScriptStepInfo().step_action.compare("ScreenOnePlate") == 0)
		return executeScreenOnePlate(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact, kg);

	else if (Tr.getCurrentScriptStepInfo().step_action.compare("Phenotype") == 0) {
		int dummy_num_worms = 0;
		vector<Phenotype> dummy_phenotypes;
		return executePhenotype(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact, dummy_num_worms, dummy_phenotypes);
	}
	
	// Below are test scripts in TestScripts.cpp
	else if (Tr.getCurrentScriptStepInfo().step_action.compare("TestCapSensor") == 0) { 
		return executeLowerPicktoTouch(Pk, Tr, Ox, Grbl2, worms, oxLock, grbl2Lock, camFact);
	}
	else if (Tr.getCurrentScriptStepInfo().step_action.compare("TestGrabbers") == 0) {
		return executeTestAutoLidHandler(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);
	}
	else if (Tr.getCurrentScriptStepInfo().step_action.compare("AutoFocus") == 0) {
		return executeAutoFocus(Ox, worms, camFact, Tr);
	}
	else if (Tr.getCurrentScriptStepInfo().step_action.compare("TestPhenotyping") == 0) {
		return executeTestPhenotyping(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);
	}
	else if (Tr.getCurrentScriptStepInfo().step_action.compare("TestStimulate") == 0) {
		return executeTestBlueLightStimulus(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);
	}
	else if (Tr.getCurrentScriptStepInfo().step_action.compare("FollowWorms") == 0) {
		return executeFindAndFollowWorms(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);
	}
	else if (Tr.getCurrentScriptStepInfo().step_action.compare("MRCNNSegWorm") == 0) {
		return executeMaskRCNN(worms);
	}
	else if (Tr.getCurrentScriptStepInfo().step_action.compare("TestDyDOx") == 0) {
		return executeTestDyDOxCalib(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);
	}
	else if (Tr.getCurrentScriptStepInfo().step_action.compare("TestCaliTray") == 0) {
		return executeTrayCalibration(Tr);
	}
	else if (Tr.getCurrentScriptStepInfo().step_action.compare("ReadBarcode") == 0) {
		return executeTestReadBarcode(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact, plateTracker);
	}
	else if (Tr.getCurrentScriptStepInfo().step_action.compare("SleepAssay") == 0) {
		return executeSleepAssay(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact, sock_ptr);
	}
	else if (Tr.getCurrentScriptStepInfo().step_action.compare("MsrLatency") == 0) {
		return executeMeasureImgLatency(Tr, Grbl2, camFact, worms);
	}
	else if (Tr.getCurrentScriptStepInfo().step_action.compare("GeneMapAnaly") == 0) {
		return executeGeneticMappingAnalysis(worms);
	}
	else if (Tr.getCurrentScriptStepInfo().step_action.compare("GenoIntegAnaly") == 0) {
		return executeGenomicIntegFluoAnalysis(worms);
	}
	else if (Tr.getCurrentScriptStepInfo().step_action.compare("FindWrMtlWell") == 0) {
		return executeFindWorMotelWell(Tr);
	}
	else {
		Err.notify(errors::invalid_script_step);
		return false;
	}
}


// DEPRECATED: Lid handling is now managed by LidHandlers class, and is automatically dealt with when called moveToPlate with lid handling script arg.
//bool executeLiftLid(WormPick &Pk, OxCnc &Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock) {
//	
//	// Move the pick out of the way
//	Pk.movePickSlightlyAway();
//
//	// CNC lift
//	Ox.liftOrLowerLid();
//
//	// Return pick to usable position
//	/// Do this locally in each pick method
//
//	return true;
//}

//bool executeLowerLid(WormPick &Pk, OxCnc &Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, bool return_to_plate) {
//	
//	//cout << "Dummy: LowerLid" << endl;
//	//return true;
//
//	// Move pick out of the way
//	Pk.movePickSlightlyAway();
//
//	// CNC lower
//	Ox.lowerLid(return_to_plate);
//	return true;
//
//}


//// This is the old deprecated versions of executePickNWorms... the current executePickNWorms is what was originally called executePickNWormsV2, but it is better so it replaced this method.
//bool executePickNWorms(WormPick &Pk, PlateTray& Tr, OxCnc &Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder& worms, int& kg_or_rflag_Script_thread_isReady) {
//
//	// args[0]: number: total amount of worms
//	int number = Tr.getCurrentScriptStepInfo().args[0];
//
//	// args[1]: contain: worms per plate
//	int contain = Tr.getCurrentScriptStepInfo().args[1];
//
//	// args[2] and args[3]: putrow, putcolumn: location of where we put the worm.
//	int putrow = Tr.getCurrentScriptStepInfo().args[2];
//	int putcolumn = Tr.getCurrentScriptStepInfo().args[3];
//
//	// args[4] is 0 - Verify picking(dropping) event (success or not) by automatic assessment
//	// args[4] is 1 - Verify picking(dropping) event (success or not) by human assessment
//	bool assess_mode = Tr.getCurrentScriptStepInfo().args[4];
//
//	// args[5]: Number of picking/dropping tries allowed
//	int num_try = Tr.getCurrentScriptStepInfo().args[5];
//
//	// pickid: location of source plate where worms are picked
//	MatIndex pickid = Tr.getCurrentScriptStepInfo().id;
//	PlateTray dummy_tray(Tr);
//
//
//	// File that records the timing of each step
//	string tFile = "C:\\WormPicker\\Timing\\Timing_20210113.txt";
//	ofstream TimeData;
//	TimeData.open(tFile, std::fstream::app);
//
//	// Measure the time of each step
//	AnthonysTimer Ta;
//
//	for (int i = 0; i < number; i++) {
//
//		cout << "****** Start transferring worm " << i << " ******"<< endl;
//		// Time for sterilization
//		Ta.setToZero();
//		double t_0 = Ta.getElapsedTime();
//		Ta.printElapsedTime("Time at beginning of = ");
//		TimeData << endl;
//		TimeData << "StartTransfer" << i << "\t" << t_0 << "\t";
//
//		// changes Tr's plate id back to original
//		dummy_tray.changeScriptId(pickid);
//		dummy_tray.updateStepInfo();
//
//		// Sterilize the pick
//		dummy_tray.changeScriptArgs({5, 10, 0.15, 0, 0, 0, 0, 0, 0, 0});
//		executeHeatPick(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock);
//		// Time for sterilization
//		double t_ster = Ta.getElapsedTime();
//		Ta.printElapsedTime("Time for Sterilization = ");
//		TimeData << "Sterilization\t" << t_ster << "\t";
//
//		// Go to the source plate
//		dummy_tray.resetScriptArgs(); // reset arguments to be zeros
//		executeMoveAndLift(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock);
//		// Time for moveandlift source plate
//		double t_MLS = Ta.getElapsedTime();
//		Ta.printElapsedTime("Time for MoveAndLift source plate = ");
//		TimeData << "MoveAndLiftSrc\t" << t_MLS << "\t";
//
//
//		//// Search worm over the plate
//		//dummy_tray.changeScriptArgs({ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 });
//		//if (!executePlateMapping(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms)) {
//		//	cout << "No worm found on the source plate!" << endl;
//		//	break;
//		//}
//
//
//		// Find a pickable worm on the plate
//		dummy_tray.changeScriptArgs({ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 });
//		if (!executeFindWorm(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms)) {
//
//			double t_End = Ta.getElapsedTime();
//			Ta.printElapsedTime("Time for FindWorm at source plate (No worm found)= ", true);
//			TimeData << "EndfromFindWorm\t" << t_End << "\t";
//			break;
//		}
//		// Time for FindWorm at source plate
//		double t_FW = Ta.getElapsedTime();
//		Ta.printElapsedTime("Time for FindWorm at source plate = ");
//		TimeData << "FindWorm\t" << t_FW << "\t";
//
//
//		cout << "Pick a worm..." << endl;
//		// changes the script args to be sutiable for picking the worm
//		// Allow multiple tries to pick a worm
//		bool success;
//		for (int tries = 0; tries < num_try; tries++) {
//
//			cout << "Start Picking Attempt " << tries << endl;
//
//			// Time at the beginning of the attempt
//			double t_begin = Ta.getElapsedTime();
//			Ta.printElapsedTime("Time at beginning of picking attempt = ");
//			TimeData << "PkAtpt" << tries << "\t" << t_begin << "\t";
//
//			// Center a worm
//			dummy_tray.resetScriptArgs();
//			// executeCenterWorm(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, kg_or_rflag_Script_thread_isReady);
//			if (!executeCenterWorm(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, kg_or_rflag_Script_thread_isReady)) {
//				success = false;
//				break;
//			}
//			// Time for centering worm
//			double t_CW = Ta.getElapsedTime();
//			Ta.printElapsedTime("Time for centering worm = ");
//			TimeData << "CenteringWorm" << "\t" << t_CW << "\t";
//
//
//			// Machine assessment
//			bool mach_test = executespeedPick(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms);
//			success = mach_test;
//
//			// Process of manual assessment
//			if (assess_mode) {
//				bool manual_test = true;
//				string input_fragment;
//				cout << "Worm picked up?    0(N)  -or-  ENTER(Y)...\n\n";
//				getline(cin, input_fragment);
//				if (input_fragment.size() > 0) { manual_test = false; }
//				success = manual_test;
//			}
//
//			// If success stop trying again
//			if (success) {
//				break;
//			}
//		}
//
//		executeLowerLid(Pk, Ox, Grbl2, oxLock, grbl2Lock, true);
//		double t_LS = Ta.getElapsedTime();
//		Ta.printElapsedTime("Time for LoweringLid = ");
//		TimeData << "LowerLid" << "\t" << t_LS << "\t";
//
//		if (!success) {
//
//			continue;
//		}
//
//		//move to the destination plate
//		cout << "Drop a worm..." << endl;
//		MatIndex nextplateid = MatIndex(putrow, putcolumn);
//		dummy_tray.changeScriptId(nextplateid);
//		dummy_tray.updateStepInfo();
//
//		//reset arguments
//		dummy_tray.resetScriptArgs();
//		executeMoveAndLift(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock);
//		//executeCenterWorm(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, kg_or_rflag_Script_thread_isReady);
//
//		// Time for moveandlift destination plate
//		double t_MLD = Ta.getElapsedTime();
//		Ta.printElapsedTime("Time for MoveAndLift destination plate = ");
//		TimeData << "MoveAndLiftDst\t" << t_MLD << "\t";
//
//
//		// Focus image
//		dummy_tray.resetScriptArgs();
//		executeFocusImage(Pk, dummy_tray, Ox, oxLock, worms);
//		double t_Foc = Ta.getElapsedTime();
//		Ta.printElapsedTime("Time for Autofocusing at destination plate = ");
//		TimeData << "FocDst\t" << t_Foc << "\t";
//
//		// changes the script args to be sutiable for dropping the worm
//		dummy_tray.changeScriptArgs({ 3, -15, 2, 0, 1, 0, 0, 0, 0, 0 });
//		for (int tries = 0; tries < num_try; tries++) {
//
//
//
//			cout << "Start Unloading Attempt " << tries << endl;
//
//			// Time at the beginning of the unloading attempt
//			double t_drop_begin = Ta.getElapsedTime();
//			Ta.printElapsedTime("Time at beginning of unloading attempt = ");
//			TimeData << "UnLdAtpt" << tries << "\t" << t_drop_begin << "\t";
//
//			bool success;
//
//			// Machine assessment
//			bool mach_test = executespeedPick(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms);
//			success = mach_test;
//
//			// Process of manual assessment
//			if (assess_mode) {
//				bool manual_test = true;
//				string input_fragment;
//				cout << "Worm dropped down?    0(N)  -or-  ENTER(Y)...\n\n";
//				getline(cin, input_fragment);
//				if (input_fragment.size() > 0) { manual_test = false; }
//				success = manual_test;
//			}
//
//			// If success stop trying again
//			if (success) {
//				break;
//			}
//		}
//		executeLowerLid(Pk, Ox, Grbl2, oxLock, grbl2Lock, true);
//		double t_LD = Ta.getElapsedTime();
//		Ta.printElapsedTime("Time for LoweringLid = ");
//		TimeData << "LowerLid (End)" << "\t" << t_LD << "\t";
//
//		//update the pickid
//		if ((i + 1) % contain == 0) {
//			putcolumn++;
//			//update the put ID
//			if (putcolumn > Tr.getNumColumns() - 1) {
//				putcolumn = 0;
//				putrow--; // TO test
//			}
//		}
//
//	}
//
//	return true;
//}

/*
Crosses worms matching specified phenotypes from 2 source plates into a destination plate
Script args:
	id: row and col of first source plate
	arg0, arg1: row and col of second source plate 
	arg2, arg3: row and col of destination plate
	arg4, arg5: row and col of intermediate picking plate
*/
bool executeCross(WormPick &Pk, PlateTray &Tr, OxCnc &Ox, OxCnc &Grbl2,
	StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock,
	WormFinder& worms, CameraFactory& camFact, int& kg) {

	// Record the important data of the experiment as it runs
	string tFile = "C:\\WormPicker\\recordings\\Experiments\\";
	string date_string = getCurrentDateString();
	string datetime_string = getCurrentTimeString(false, true);
	string exp_title = "experiment";
	if (global_exp_title != "") {
		exp_title = global_exp_title;
	}
	tFile.append(exp_title); tFile.append("_"); 
	tFile.append(date_string); tFile.append(".txt");
	experiment_data.open(tFile, std::fstream::app);
	
	experiment_data << endl << "============================================" << endl;
	experiment_data << "Experiment title: " << global_exp_title << endl;
	experiment_data << "Experiment started: " << datetime_string << endl << endl;

	// Ask user for cross parameters (what phenotyping they would like to do)
	vector<int> num_worms_from_sources;
	vector<Phenotype> cross_phenotypes = getWantedPhenotypeFromInput(2, &num_worms_from_sources);

	//TODO: print the phenotyping method object to file
	/*experiment_data << "User entered desired phenotyping as follows:" << endl;
	unordered_map<int, vector<phenotype_options>>::iterator p;
	for (p = cross_parameters.begin(); p != cross_parameters.end(); p++){
		experiment_data << "\tSource plate " << p->first << ": ";
		for (auto t : p->second) {
			experiment_data << t << ", ";
		}
		experiment_data << endl;
	}
	experiment_data << endl;*/

	// Get the number of worms to pick from each source plate
	int source1_num = num_worms_from_sources[0];
	int source2_num = num_worms_from_sources[1];

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

	//args[4], args[5] row and col, respectively, of the intermediate plate
	int int_row = stoi(Tr.getCurrentScriptStepInfo().args[4]);
	int int_col = stoi(Tr.getCurrentScriptStepInfo().args[5]);

	/*
	//experiment_data << "Removing plate lids and placing them on empty plates for safe keeping until experiment is over...";
	//// Now go to each plate in the experiment, remove its lid, and drop it one row up for safekeeping.
	//vector<tuple<MatIndex, MatIndex>> start_exp_lid_maneuvers;
	//start_exp_lid_maneuvers.push_back(tuple<MatIndex, MatIndex>(source1_loc, MatIndex(source1_row - 1, source1_col)));
	//start_exp_lid_maneuvers.push_back(tuple<MatIndex, MatIndex>(source2_loc, MatIndex(source2_row - 1, source2_col)));
	//start_exp_lid_maneuvers.push_back(tuple<MatIndex, MatIndex>(dest_loc, MatIndex(dest_row - 1, dest_col)));
	//start_exp_lid_maneuvers.push_back(tuple<MatIndex, MatIndex>(int_loc, MatIndex(int_row - 1, int_col)));
	//executeLiftAndDropLids(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, start_exp_lid_maneuvers);
	//experiment_data << "done removing lids!" << endl;
	*/

	// Dummy tray object for preparing script
	PlateTray dummy_tray(Tr);

	// Timers
	AnthonysTimer step_timer;
	AnthonysTimer total_timer;

	// Preparing the script for picking from source plate 1 to destination
	dummy_tray.changeScriptId(source1_loc);
	dummy_tray.updateStepInfo();
	vector<string> Args_pick_from_src1_to_dest = {
		to_string(dest_row),												//	arg0 in PickNWorms
		to_string(dest_col),												//	arg1
		to_string(int_row),												//	arg2
		to_string(int_col),												//	arg3
		to_string(num_worms_from_sources[0]),								//	arg4
		to_string(static_cast<int>(cross_phenotypes[0].SEX_type)),			//	arg5
		to_string(static_cast<int>(cross_phenotypes[0].GFP_type)),			//	arg6
		to_string(static_cast<int>(cross_phenotypes[0].RFP_type)),			//	arg7
		to_string(static_cast<int>(cross_phenotypes[0].MORPH_type)),		//	arg8
		to_string(static_cast<int>(cross_phenotypes[0].STAGE_type))		//	arg9
	};
	dummy_tray.changeScriptArgs(Args_pick_from_src1_to_dest);			// set move inputs

	//Picking from source plate 1 to destination
	cout << "============= Picking from Source 1 ==============" << endl;
	experiment_data << "Attempting picking worm from source plate 1." << endl;
	executePickNWorms(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact, kg, false);

	// Count the time for picking from source 1 to destination
	experiment_data << "Finished picking from source plate 1! Elapsed time: " << step_timer.getElapsedTime() << endl;
	step_timer.setToZero();

	// Preparing the script for picking from source plate 2 to destination
	dummy_tray.changeScriptId(source2_loc);
	dummy_tray.updateStepInfo();
	vector<string> Args_pick_from_src2_to_dest = {
		to_string(dest_row),												//	arg0 in PickNWorms
		to_string(dest_col),												//	arg1
		to_string(int_row),												//	arg2
		to_string(int_col),												//	arg3
		to_string(num_worms_from_sources[1]),								//	arg4
		to_string(static_cast<int>(cross_phenotypes[1].SEX_type)),			//	arg5
		to_string(static_cast<int>(cross_phenotypes[1].GFP_type)),			//	arg6
		to_string(static_cast<int>(cross_phenotypes[1].RFP_type)),			//	arg7
		to_string(static_cast<int>(cross_phenotypes[1].MORPH_type)),		//	arg8
		to_string(static_cast<int>(cross_phenotypes[1].STAGE_type))		//	arg9
	};
	dummy_tray.changeScriptArgs(Args_pick_from_src2_to_dest); // set move inputs

	//Picking from source plate 1 to destination
	executePickNWorms(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact, kg, false);
	// Count the time for picking from source 2 to destination
	experiment_data << "Finished picking from source plate 2! Elapsed time: " << step_timer.getElapsedTime() << endl;

	/*
	experiment_data << "Replacing lids back onto source and destination plates." << endl;
	//// Now replace the lids from eachplate in the experiment.
	//vector<tuple<MatIndex, MatIndex>> end_exp_lid_maneuvers;
	//end_exp_lid_maneuvers.push_back(tuple<MatIndex, MatIndex>(MatIndex(source1_row - 1, source1_col), source1_loc));
	//end_exp_lid_maneuvers.push_back(tuple<MatIndex, MatIndex>(MatIndex(source2_row - 1, source2_col), source2_loc));
	//end_exp_lid_maneuvers.push_back(tuple<MatIndex, MatIndex>(MatIndex(dest_row - 1, dest_col), dest_loc));
	//end_exp_lid_maneuvers.push_back(tuple<MatIndex, MatIndex>(MatIndex(int_row - 1, int_col), int_loc));
	//executeLiftAndDropLids(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, end_exp_lid_maneuvers);
	*/


	// Drop all lids
	Pk.lid_handler.dropAllLids(Tr, Ox);

	experiment_data << "Experiment over! Total time: " << total_timer.getElapsedTime() << endl;
	experiment_data.close();
	return true;
}

/*
Crosses worms matching specified phenotypes from 2 source plates into a destination plate,
with PlateTracker
Script args:
	id: row and col of first source plate
	arg0, arg1: row and col of second source plate
	arg2, arg3: row and col of destination plate
	arg4, arg5: row and col of intermediate picking plate
*/
bool executeCross(WormPick& Pk, PlateTray& Tr, OxCnc& Ox, OxCnc& Grbl2, StrictLock<OxCnc>& oxLock,
                  StrictLock<OxCnc>& grbl2Lock, WormFinder& worms, CameraFactory& camFact, int& kg,
                  PlateTracker& plateTracker)
{
	// Record the important data of the experiment as it runs
	string tFile = "C:\\WormPicker\\recordings\\Experiments\\";
	string date_string = getCurrentDateString();
	string datetime_string = getCurrentTimeString(false, true);
	string exp_title = "experiment";
	if (global_exp_title != "")
	{
		exp_title = global_exp_title;
	}
	tFile.append(exp_title);
	tFile.append("_");
	tFile.append(date_string);
	tFile.append(".txt");
	experiment_data.open(tFile, std::fstream::app);

	experiment_data << endl << "============================================" << endl;
	experiment_data << "Experiment title: " << global_exp_title << endl;
	experiment_data << "Experiment started: " << datetime_string << endl << endl;

	// Ask user for cross parameters (what phenotyping they would like to do)
	vector<int> num_worms_from_sources;
	vector<Phenotype> cross_phenotypes = getWantedPhenotypeFromInput(2, &num_worms_from_sources);

	// Get the number of worms to pick from each source plate
	int source1_num = num_worms_from_sources[0];
	int source2_num = num_worms_from_sources[1];

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

	//args[4], args[5] row and col, respectively, of the intermediate plate
	int int_row = stoi(Tr.getCurrentScriptStepInfo().args[4]);
	int int_col = stoi(Tr.getCurrentScriptStepInfo().args[5]);

	// Dummy tray object for preparing script
	PlateTray dummy_tray(Tr);

	// Timers
	AnthonysTimer step_timer;
	AnthonysTimer total_timer;

	// Preparing the script for picking from source plate 1 to destination
	dummy_tray.changeScriptId(source1_loc);
	dummy_tray.updateStepInfo();
	vector<string> Args_pick_from_src1_to_dest = {
		to_string(dest_row), //	arg0 in PickNWorms
		to_string(dest_col), //	arg1
		to_string(int_row), //	arg2
		to_string(int_col), //	arg3
		to_string(num_worms_from_sources[0]), //	arg4
		to_string(static_cast<int>(cross_phenotypes[0].SEX_type)), //	arg5
		to_string(static_cast<int>(cross_phenotypes[0].GFP_type)), //	arg6
		to_string(static_cast<int>(cross_phenotypes[0].RFP_type)), //	arg7
		to_string(static_cast<int>(cross_phenotypes[0].MORPH_type)), //	arg8
		to_string(static_cast<int>(cross_phenotypes[0].STAGE_type)) //	arg9
	};
	dummy_tray.changeScriptArgs(Args_pick_from_src1_to_dest); // set move inputs

	//Picking from source plate 1 to destination
	cout << "============= Picking from Source 1 ==============" << endl;
	experiment_data << "Attempting picking worm from source plate 1." << endl;
	executePickNWorms(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact, kg, false, plateTracker);

	// Count the time for picking from source 1 to destination
	experiment_data << "Finished picking from source plate 1! Elapsed time: " << step_timer.getElapsedTime() << endl;
	step_timer.setToZero();

	// Preparing the script for picking from source plate 2 to destination
	dummy_tray.changeScriptId(source2_loc);
	dummy_tray.updateStepInfo();
	vector<string> Args_pick_from_src2_to_dest = {
		to_string(dest_row), //	arg0 in PickNWorms
		to_string(dest_col), //	arg1
		to_string(int_row), //	arg2
		to_string(int_col), //	arg3
		to_string(num_worms_from_sources[1]), //	arg4
		to_string(static_cast<int>(cross_phenotypes[1].SEX_type)), //	arg5
		to_string(static_cast<int>(cross_phenotypes[1].GFP_type)), //	arg6
		to_string(static_cast<int>(cross_phenotypes[1].RFP_type)), //	arg7
		to_string(static_cast<int>(cross_phenotypes[1].MORPH_type)), //	arg8
		to_string(static_cast<int>(cross_phenotypes[1].STAGE_type)) //	arg9
	};
	dummy_tray.changeScriptArgs(Args_pick_from_src2_to_dest); // set move inputs

	//Picking from source plate 1 to destination
	executePickNWorms(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact, kg, false, plateTracker);
	// Count the time for picking from source 2 to destination
	experiment_data << "Finished picking from source plate 2! Elapsed time: " << step_timer.getElapsedTime() << endl;

	// Drop all lids
	Pk.lid_handler.dropAllLids(Tr, Ox);

	experiment_data << "Experiment over! Total time: " << total_timer.getElapsedTime() << endl;
	experiment_data.close();
	return true;
}

/*
Picks worms matching a specified phenotype from a single source plate onto individual destination plate. A single worm per plate.
You can specify up to 7 rows into which you will single worms. If you specify a row for singling the script will assume ALL
plates in that row (other than the source or intermediate plates) are valid plates to pick worms to.
Script args:
	id: row and col of the source row
	arg0, arg1: row and col of intermediate plate
	arg2: number of worms to single or equivalently how many plates to single to
	arg3 - arg9: rows of plates to single worms into. The script will start singling to the lowest number row, and work in ascending order.
*/
bool executeSingleWorms(WormPick &Pk, PlateTray &Tr, OxCnc &Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder& worms, CameraFactory& camFact, int& kg) {
	//Ask user for cross parameters (what phenotyping they would like to do)
	Phenotype single_phenotype = getWantedPhenotypeFromInput(1)[0];

	// plate coordinates of the source plate
	int source_row = Tr.getCurrentScriptStepInfo().id.row;
	int source_col = Tr.getCurrentScriptStepInfo().id.col;
	MatIndex source_loc(source_row, source_col);

	// plate coordinates of the intermediate plate
	int inter_row = stoi(Tr.getCurrentScriptStepInfo().args[0]);
	int inter_col = stoi(Tr.getCurrentScriptStepInfo().args[1]);
	MatIndex intermediate_loc(inter_row, inter_col);

	vector<MatIndex> excluded_locs = { source_loc, intermediate_loc };

	int total_num_plates = stoi(Tr.getCurrentScriptStepInfo().args[2]);

	// get the rows of plates we'll use as singling plates
	set<int> singling_rows;
	singling_rows.insert(stoi(Tr.getCurrentScriptStepInfo().args[3]));
	singling_rows.insert(stoi(Tr.getCurrentScriptStepInfo().args[4]));
	singling_rows.insert(stoi(Tr.getCurrentScriptStepInfo().args[5]));
	singling_rows.insert(stoi(Tr.getCurrentScriptStepInfo().args[6]));
	singling_rows.insert(stoi(Tr.getCurrentScriptStepInfo().args[7]));
	singling_rows.insert(stoi(Tr.getCurrentScriptStepInfo().args[8]));
	singling_rows.insert(stoi(Tr.getCurrentScriptStepInfo().args[9]));

	// Create a vector of all the plates we can use for singling by checking if they're valid locations
	vector<MatIndex> singling_plates = generatePlateLocations(singling_rows, Tr, Pk, excluded_locs, LidHandler::plate_type::IS_DESTINATION);

	cout << "Singling plate locs: " << endl;
	for (auto loc : singling_plates) {
		cout << "\t(" << loc.row << ", " << loc.col << ")" << endl;
	}

	if (singling_plates.size() < total_num_plates) {
		cout << "WARNING: There are " << total_num_plates << " worms that need to be singled but only " << singling_plates.size() << " plates to pick them to." << endl;
		cout << "Experiment will not be able to complete. Abandoning experiment." << endl;
		return false;
	}

	// Record the important data of the experiment as it runs
	string tFile = "C:\\WormPicker\\recordings\\Experiments\\";
	string date_string = getCurrentDateString();
	string datetime_string = getCurrentTimeString(false, true);
	string exp_title = "experiment";
	if (global_exp_title != "") {
		exp_title = global_exp_title;
	}
	tFile.append(exp_title); tFile.append("_");
	tFile.append(date_string); tFile.append(".txt");
	experiment_data.open(tFile, std::fstream::app);

	experiment_data << endl << "============================================" << endl;
	experiment_data << "Experiment title: " << global_exp_title << endl;
	experiment_data << "Experiment started: " << datetime_string << endl << endl;

	// Dummy tray object for preparing script
	PlateTray dummy_tray(Tr);

	// Initialize script arguments. The destination is set to (0,0) by default before entering the loop
	vector<string> Args_pick_from_src_to_dest = {
		to_string(0),												//	arg0 in PickNWorms
		to_string(0),												//	arg1
		to_string(inter_row),										//	arg2
		to_string(inter_col),										//	arg3
		to_string(1),												//	arg4
		to_string(static_cast<int>(single_phenotype.SEX_type)),		//	arg5
		to_string(static_cast<int>(single_phenotype.GFP_type)),		//	arg6
		to_string(static_cast<int>(single_phenotype.RFP_type)),		//	arg7
		to_string(static_cast<int>(single_phenotype.MORPH_type)),	//	arg8
		to_string(static_cast<int>(single_phenotype.STAGE_type))	//	arg9
	};
	dummy_tray.changeScriptArgs(Args_pick_from_src_to_dest);

	// Timers
	AnthonysTimer step_timer;
	AnthonysTimer total_timer;

	// Iterate throught the desitnation plates for singling
	// Just need to change the destination location in the script
	for (int i = 0; i < total_num_plates; i++) {

		// set destination
		dummy_tray.changeScriptArgs(0, to_string(singling_plates[i].row));
		dummy_tray.changeScriptArgs(1, to_string(singling_plates[i].col));

		//Picking from source plate to destination
		cout << "============= Attempting singling worm # " << i+1 << " ===============" << endl;
		experiment_data << "============= Attempting singling worm # " << i+1 << " =============== " << endl;
		executePickNWorms(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact, kg, true);


		cout << "Finished picking from source plate to destination ("
			<< singling_plates[i].row << ", " << singling_plates[i].col << ")" << endl;
		cout << "============= Finished singling worm # " << i + 1 << " ===============" << endl;

		// Count the time for picking from source to destination
		experiment_data << "Finished picking from source plate to destination ("
			<< singling_plates[i].row << ", " << singling_plates[i].col
			<< ") ! Elapsed time: " << step_timer.getElapsedTime() << endl;
		step_timer.setToZero();
	}

	// Drop all lids
	Pk.lid_handler.dropAllLids(Tr, Ox);

	experiment_data << "Experiment over! Total time: " << total_timer.getElapsedTime() << endl;
	experiment_data.close();


	exit(EXIT_SUCCESS);

	return true;
}


/*
Screens plates for the desired phenotype
You can specify up to 8 rows that contain plates to screen. If you specify a row for screening the script will assume EVERY
location in that row contains a plate to screen. 
The screen assumes no specific phenotype want to screen for, and check all the attribute, 
i.e., arg1 = 0 and arg2-6 all = 1 referring to executeScreenOnePlate
Script args:
	arg0: number of overall plates that need to be screened
	arg1: number of worms to look at on each plate
	arg2 - arg9: rows containing plates to screen. The script will start screening at the lowest number row, and work in ascending order.
*/
bool executeScreenPlates(WormPick &Pk, PlateTray &Tr, OxCnc &Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder& worms, CameraFactory& camFact, int& kg) {
	

	// number of plates to screen
	int total_num_plates = stoi(Tr.getCurrentScriptStepInfo().args[0]);
	int num_worms_to_check_per_plate = stoi(Tr.getCurrentScriptStepInfo().args[1]);

	// get the rows that contain plates to screen
	set<int> rows_with_plates;
	for (int i = 2; i < 10; ++i) {
		rows_with_plates.insert(stoi(Tr.getCurrentScriptStepInfo().args[i]));
	}

	// Create a vector of all the plates we can use for singling by checking if they're valid locations
	vector<MatIndex> plate_locs = generatePlateLocations(rows_with_plates, Tr, Pk);

	if (plate_locs.size() < total_num_plates) {
		cout << "WARNING: There are " << total_num_plates << " plates that need to be screened but only " << plate_locs.size() << " plates in maximum can be put on the specified rows." << endl;
		cout << "Experiment will not be able to complete. Abandoning experiment." << endl;
		return false;
	}

	// Get a dummy copy
	PlateTray dummy_tray(Tr);

	// Initialize script arguments. The script ID is set to (0,0) by default before entering the loop
	dummy_tray.changeScriptId(MatIndex(0,0));

	vector<string> Args_screen_plates = {
		to_string(num_worms_to_check_per_plate),					//	arg0 in ScreenOnePlate
		to_string(0),												//	arg1
		to_string(1),												//	arg2
		to_string(1),												//	arg3
		to_string(1),												//	arg4
		to_string(1),												//	arg5
		to_string(1),												//	arg6
	};
	dummy_tray.changeScriptArgs(Args_screen_plates);

	// Iterate throught the desitnation plates for screnning
	// Just need to change the screen location (script ID) in the script
	for (int i = 0; i < total_num_plates; ++i) {
		cout << "Start to screen plate #" << i + 1 << " of " << total_num_plates << "." << endl;
		dummy_tray.changeScriptId(plate_locs[i]);

		// Screen a single plate
		executeScreenOnePlate(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact, kg);
	}

	// Put down all the lids
	Pk.lid_handler.dropAllLids(Tr, Ox);


	return true;
}



// DEPRECATED: the PickNWorms script was rewritten based on new utilities we have, 
// e.g. autofocus, mask-rcnn worm segment, new version of lid handling.
// The new version of PickNWorms is a mid-level script that can be called by high-level scripts.
// Test script for serial transfer: worm are transferred WITHOUT lid handling

//bool executePickNWorms(WormPick &Pk, PlateTray& Tr, OxCnc &Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder &worms, CameraFactory& camFact, int& kg) {
//
//
//
//	cout << "PickNworm" << endl;
//
//	// args[0]: number: total amount of worms
//	int number = Tr.getCurrentScriptStepInfo().args[0];
//
//	// args[1]: contain: worms per plate
//	int contain = Tr.getCurrentScriptStepInfo().args[1];
//
//	// args[2] and args[3]: putrow, putcolumn: location of where we put the worm.
//	int putrow = Tr.getCurrentScriptStepInfo().args[2];
//	int putcolumn = Tr.getCurrentScriptStepInfo().args[3];
//
//	// args[4] is 0 - Verify picking(dropping) event (success or not) by automatic assessment
//	// args[4] is 1 - Verify picking(dropping) event (success or not) by human assessment
//	bool assess_mode = Tr.getCurrentScriptStepInfo().args[4];
//
//	// args[5]: Number of picking/dropping tries allowed
//	int num_try = Tr.getCurrentScriptStepInfo().args[5];
//
//	// pickid: location of source plate where worms are picked
//	MatIndex pickid = Tr.getCurrentScriptStepInfo().id;
//	PlateTray dummy_tray(Tr);
//
//
//	// File that records the timing of each step
//	string tFile = "C:\\WormPicker\\Timing\\Timing_20210307.txt";
//	ofstream TimeData;
//	TimeData.open(tFile, std::fstream::app);
//
//	// Measure the time of each step
//	AnthonysTimer Ta;
//
//	for (int i = 0; i < number; i++) {
//
//		cout << "****** Start transferring worm " << i << " ******" << endl;
//		// Time for sterilization
//		Ta.setToZero();
//		double t_0 = Ta.getElapsedTime();
//		Ta.printElapsedTime("Time at beginning of = ");
//		TimeData << endl;
//		TimeData << "StartTransfer" << i << "\t" << t_0 << "\t";
//
//		// changes Tr's plate id back to original
//		dummy_tray.changeScriptId(pickid);
//		dummy_tray.updateStepInfo();
//
//		//// Sterilize the pick
//		//dummy_tray.changeScriptArgs({ 5, 10, 0.15, 0, 0, 0, 0, 0, 0, 0 });
//		//executeHeatPick(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock);
//		//// Time for sterilization
//		//double t_ster = Ta.getElapsedTime();
//		//Ta.printElapsedTime("Time for Sterilization = ");
//		//TimeData << "Sterilization\t" << t_ster << "\t";
//
//		// Go to the source plate
//		dummy_tray.changeScriptArgs({ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 }); // reset arguments to be zeros // To check arg0
//		executeMoveToPlate(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);
//		// executeMoveAndLift(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock);
//		// Time for moveandlift source plate
//		double t_MLS = Ta.getElapsedTime();
//		Ta.printElapsedTime("Time for MoveAndLift source plate = ");
//		TimeData << "MoveAndLiftSrc\t" << t_MLS << "\t";
//
//
//		//// Search worm over the plate
//		//dummy_tray.changeScriptArgs({ 1, 0, 0, 0, 0, 0 });
//		//if (!executePlateMapping(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms)) {
//		//	cout << "No worm found on the source plate!" << endl;
//		//	break;
//		//}
//
//
//		// Find a pickable worm on the plate
//		dummy_tray.changeScriptArgs({ 1, 0, 0, 0, 0, 0.15, 0, 0, 0, 0 });
//		if (!executeFindWorm(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms)) {
//
//			double t_End = Ta.getElapsedTime();
//			Ta.printElapsedTime("Time for FindWorm at source plate (No worm found)= ", true);
//			TimeData << "EndfromFindWorm\t" << t_End << "\t";
//			break;
//		}
//		// Time for FindWorm at source plate
//		double t_FW = Ta.getElapsedTime();
//		Ta.printElapsedTime("Time for FindWorm at source plate = ");
//		TimeData << "FindWorm\t" << t_FW << "\t";
//
//
//		cout << "Pick a worm..." << endl;
//		// changes the script args to be sutiable for picking the worm
//		// Allow multiple tries to pick a worm
//		bool success;
//		for (int tries = 0; tries < num_try; tries++) {
//
//			cout << "Start Picking Attempt " << tries << endl;
//
//			// Time at the beginning of the attempt
//			double t_begin = Ta.getElapsedTime();
//			Ta.printElapsedTime("Time at beginning of picking attempt = ");
//			TimeData << "PkAtpt" << tries << "\t" << t_begin << "\t";
//
//			// Center a worm
//			dummy_tray.resetScriptArgs();
//			// executeCenterWorm(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, kg_or_rflag_Script_thread_isReady);
//			// Check tracked worm is empty
//			vector<phenotype_options> dummy_result;
//			if (!executeCenterWorm(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact, kg, dummy_result)) {
//				success = false;
//				break;
//			}
//			// Time for centering worm
//			double t_CW = Ta.getElapsedTime();
//			Ta.printElapsedTime("Time for centering worm = ");
//			TimeData << "CenteringWorm" << "\t" << t_CW << "\t";
//
//
//			// Machine assessment
//			// bool mach_test = executeCNNtrackPickRapid(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms);
//			dummy_tray.changeScriptArgs({ 3, 9, 0, 0, 1, 0, 0, 0, 0, 0 });
//			bool mach_test = executeSpeedPick(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);
//
//			// Time for Picking attempt
//			double t_P = Ta.getElapsedTime();
//			Ta.printElapsedTime("Time for Picking = ");
//			TimeData << "Picking" << "\t" << t_P << "\t";
//
//			success = mach_test;
//
//			// Process of manual assessment
//			if (assess_mode) {
//				bool manual_test = true;
//				string input_fragment;
//				cout << "Worm picked up?    0(N)  -or-  ENTER(Y)...\n\n";
//				getline(cin, input_fragment);
//				if (input_fragment.size() > 0) { manual_test = false; }
//				success = manual_test;
//			}
//
//			// If success stop trying again
//			if (success) {
//				break;
//			}
//		}
//
//		// Lowerlid
//		/*executeLowerLid(Pk, Ox, Grbl2, oxLock, grbl2Lock, true);
//		double t_LS = Ta.getElapsedTime();
//		Ta.printElapsedTime("Time for LoweringLid = ");
//		TimeData << "LowerLid" << "\t" << t_LS << "\t";*/
//
//		if (!success) {
//
//			continue;
//		}
//
//		//move to the destination plate
//		cout << "Drop a worm..." << endl;
//		MatIndex nextplateid = MatIndex(putrow, putcolumn);
//		dummy_tray.changeScriptId(nextplateid);
//		dummy_tray.updateStepInfo();
//
//		//Set arguments
//		dummy_tray.changeScriptArgs({ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 });
//		executeMoveToPlate(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact); // To check: arg[0]
//		// executeMoveAndLift(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock);
//		//executeCenterWorm(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, kg_or_rflag_Script_thread_isReady);
//
//		// Time for moveandlift destination plate
//		double t_MLD = Ta.getElapsedTime();
//		Ta.printElapsedTime("Time for MoveAndLift destination plate = ");
//		TimeData << "MoveAndLiftDst\t" << t_MLD << "\t";
//
//
//		// Focus image
//		/*dummy_tray.resetScriptArgs();
//		executeFocusImage(Pk, dummy_tray, Ox, oxLock, worms);
//		double t_Foc = Ta.getElapsedTime();
//		Ta.printElapsedTime("Time for Autofocusing at destination plate = ");
//		TimeData << "FocDst\t" << t_Foc << "\t";*/
//
//		// changes the script args to be sutiable for dropping the worm
//		dummy_tray.changeScriptArgs({ 3, -3, 1.5, 0, 1, 0, 0, 0, 0, 0 });
//		for (int tries = 0; tries < num_try; tries++) {
//
//
//
//			cout << "Start Unloading Attempt " << tries << endl;
//
//			// Time at the beginning of the unloading attempt
//			double t_drop_begin = Ta.getElapsedTime();
//			Ta.printElapsedTime("Time at beginning of unloading attempt = ");
//			TimeData << "UnLdAtpt" << tries << "\t" << t_drop_begin << "\t";
//
//			bool success;
//
//			// Machine assessment
//			bool mach_test = executeSpeedPick(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);
//
//			// Time for Picking attempt
//			double t_Drop = Ta.getElapsedTime();
//			Ta.printElapsedTime("Time for Unloading = ");
//			TimeData << "Unloading" << "\t" << t_Drop << "\t";
//
//			success = mach_test;
//
//			// Process of manual assessment
//			if (assess_mode) {
//				bool manual_test = true;
//				string input_fragment;
//				cout << "Worm dropped down?    0(N)  -or-  ENTER(Y)...\n\n";
//				getline(cin, input_fragment);
//				if (input_fragment.size() > 0) { manual_test = false; }
//				success = manual_test;
//			}
//
//			// If success stop trying again
//			if (success) {
//				break;
//			}
//		}
//
//		// Lower lid
//		/*executeLowerLid(Pk, Ox, Grbl2, oxLock, grbl2Lock, true);
//		double t_LS = Ta.getElapsedTime();
//		Ta.printElapsedTime("Time for LoweringLid = ");
//		TimeData << "LowerLid (End)" << "\t" << t_LS << "\t";*/
//
//		//update the pickid
//		if ((i + 1) % contain == 0) {
//			putcolumn++;
//			//update the put ID
//			if (putcolumn > Tr.getNumColumns() - 1) {
//				putcolumn = 0;
//				putrow--; // TO test
//			}
//		}
//
//	}
//
//	return true;
//}

/*
executePickNWorms: a mid-level script that transfers multiple (N) worms with certain phenotypes from one source to one destination.
The mid-level script is called by high-level scripts
Script args:
	id:			row and col of the source plate
	arg0, arg1:	row and col of destination plate
	arg2, arg3: row and col of intermediate plate
	arg4:		number of worms want to transfer from source to destination
	arg5:		type of sex wanted
	arg6:		type of GFP wanted
	arg7:		type of RFP wanted
	arg8:		type of morphology wanted
	arg9:		type of developmental stage
*/

bool executePickNWorms(WormPick &Pk, PlateTray& Tr, OxCnc &Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, 
	StrictLock<OxCnc> &grbl2Lock, WormFinder &worms, CameraFactory& camFact, int& kg, bool verify_drop_over_whole_plate)
{

	// Get the location of source plate
	int source_row = Tr.getCurrentScriptStepInfo().id.row;
	int source_col = Tr.getCurrentScriptStepInfo().id.col;
	MatIndex source_loc(source_row, source_col);

	cout << "PickNWorm: row = " << source_row << ", col = " << source_col << endl;

	// Get the location of destination plate
	int dest_row = stoi(Tr.getCurrentScriptStepInfo().args[0]);
	int dest_col = stoi(Tr.getCurrentScriptStepInfo().args[1]);
	MatIndex dest_loc(dest_row, dest_col);
	Pk.lid_handler.setPlateType(dest_loc, LidHandler::IS_DESTINATION);

	// Get the location of intermediate picking plate
	int inter_row = stoi(Tr.getCurrentScriptStepInfo().args[2]);
	int inter_col = stoi(Tr.getCurrentScriptStepInfo().args[3]);
	MatIndex inter_loc(inter_row, inter_col);

	// Get the number of worms to transfer
	int num_worm_to_pick = stoi(Tr.getCurrentScriptStepInfo().args[4]);


	// Get phenotyping parameters from the argument list
	int sex_type_idx = stoi(Tr.getCurrentScriptStepInfo().args[5]);
	int GFP_type_idx = stoi(Tr.getCurrentScriptStepInfo().args[6]);
	int RFP_type_idx = stoi(Tr.getCurrentScriptStepInfo().args[7]);
	int morphology_type_idx = stoi(Tr.getCurrentScriptStepInfo().args[8]);
	int stage_type_idx = stoi(Tr.getCurrentScriptStepInfo().args[9]);


	// check input index is valid or not
	// TODO: Check whether index of plate is OOB
	if (
		!(sex_type_idx >= 0 && sex_type_idx < DUMMY_COUNT_SEX_TYPE) ||
		!(GFP_type_idx >= 0 && GFP_type_idx < DUMMY_COUNT_GFP_TYPE) ||
		!(RFP_type_idx >= 0 && RFP_type_idx < DUMMY_COUNT_RFP_TYPE) ||
		!(morphology_type_idx >= 0 && morphology_type_idx < DUMMY_COUNT_MORPH_TYPE)||
		!(stage_type_idx >= 0 && stage_type_idx < DUMMY_COUNT_STAGE_TYPE)
		) {
		cout << "Invalid idexing for the phenotyping option!" << endl;
		return false;
	}


	int num_worms_in_himag = 1; // This is passed as a reference when finding worms, it will contain the number of worms in
					   // the high mag camera, so we know if we need to do an intermediate pick or not.


	// Construct a object holding the phenotyping parameter based on the desired phenotypes
	phenotyping_method_being_used = phenotyping_method(
		Phenotype(
			(sex_types)sex_type_idx,
			(GFP_types)GFP_type_idx,
			(RFP_types)RFP_type_idx,
			(morph_types)morphology_type_idx,
			(stage_types)stage_type_idx
		)
	);

	// dummy tray object for preparing script params later on
	PlateTray dummy_tray(Tr);


	for (int i = 0; i < num_worm_to_pick; i++) {

		cout << "Attempting to find, pick, and drop worm: " << i << endl;
		if (experiment_data.is_open()) experiment_data << "Attempting worm " << i + 1 << " of " << num_worm_to_pick << " from the source plate." << endl;

		// Move to picking plate
		cout << "============= Moving to source ==============" << endl;
		dummy_tray.changeScriptId(source_loc);
		dummy_tray.updateStepInfo();

		dummy_tray.changeScriptArgs(MOVE_TO_SOURCE_DEFAULT); // set move inputs
		executeMoveToPlate(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);

		if (experiment_data.is_open()) experiment_data << "\tAt source plate." << endl;

		// Find and center a worm to pick
		cout << "============= Finding and centering a worm ==============" << endl;
		vector<Phenotype> phenotypes_of_worm_centered;
		int num_tries = 0;
		int max_tries = 30;
		bool success = false;

		// Try multiple time to find and center a worm w/ desired phenotype
		do {
			cout << "Attempting to find and center a worm for picking. Try: " << num_tries + 1 << endl;
			if (experiment_data.is_open()) 
				experiment_data << "\tAttempt " << num_tries + 1 << " of " << max_tries << " to find and center a worm for picking." << endl;

			dummy_tray.changeScriptArgs(CENTER_WORMS_DEFAULT); // set centerworm inputs
			success = executeCenterWorm(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact, kg, phenotypes_of_worm_centered, num_worms_in_himag);
			num_tries++;
			if (!success) {
				cout << "\tFailed to find and center a worm for picking." << endl;
				if (experiment_data.is_open()) experiment_data << "\tFailed to find and center a worm for picking." << endl;
			}
			cout << "------------------- Num worms found: " << num_worms_in_himag << endl;
		} while (!success && num_tries < max_tries);

		// Give up finding worm
		if (!success) {
			cout << "Failed to select a worm during the execute center worm step." << endl;
			if (experiment_data.is_open()) experiment_data << "\tFailed to select a worm during the center worm step.";
			return false;
		}

		// up to this point a worm w/ desired phenotype has been centered
		if (experiment_data.is_open()) experiment_data << "\tWorm has been selected for picking." << endl;
		cout << "\tWorm has been selected for picking." << endl;


		// Pick the worm
		cout << "============= Picking a worm ==============" << endl;
		dummy_tray.changeScriptArgs(PICK_UP_WORMS_DEFAULT); // set pickworm inputs
		executeSpeedPick(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);

		// If more than one worm in high-mag cam, then pick to intermediate plate before going to destination
		bool int_pick_success = true;
		while (num_worms_in_himag > 1) {
			int_pick_success = performIntermediatePickingStep(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact, kg, num_worms_in_himag, inter_loc, phenotypes_of_worm_centered);
			if (!int_pick_success) break;
		}

		if (!int_pick_success) {
			// We failed the intermediate picking action, so just pick up where we left off on the current plate
			i--; // decrement i so when we loop we are attempting the same worm number as before (because we didn't actually get the worm so we don't want to try for the next worm)
			continue;
		}

		if (experiment_data.is_open()) experiment_data << "\tPicking action complete. Moving to destination plate." << endl;
		cout << "\tPicking action complete. Moving to destination plate." << endl;

		// boost_sleep_ms(1000); // Just to get a quick look at the plate after the pick to see if we think it was successful or not.
		// Move to the destination plate
		cout << "============= Moving to destination ==============" << endl;
		dummy_tray.changeScriptId(dest_loc);
		dummy_tray.updateStepInfo();
		dummy_tray.changeScriptArgs(MOVE_TO_DESTINATION_DEFAULT); // set move inputs
		if (verify_drop_over_whole_plate) dummy_tray.changeScriptArgs(2, 0); // Do not add random offset while singling worms
		executeMoveToPlate(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);

		cout << "\tAt destination plate. Attempting to drop the worm." << endl;
		if (experiment_data.is_open()) experiment_data << "\tAt destination plate. Attempting to drop the worm." << endl;


		// Drop the worm at the destination plate
		cout << "============= Dropping worm at destination ==============" << endl;
		//worms.overlayflag = false;

		// set pickworm inputs
		if (verify_drop_over_whole_plate) dummy_tray.changeScriptArgs(DROP_OFF_WORMS_ON_EMPTY_DEFAULT); 
		else dummy_tray.changeScriptArgs(DROP_OFF_WORMS_DEFAULT);

		bool verified_drop = executeSpeedPick(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);
		//worms.overlayflag = true;
		if (!verified_drop) { i--; } // We didn't verify the worm drop we will try again for the same type of worm (don't increment when loop)
	}


	// script success upon this point
	return true;

}

bool executePickNWorms(WormPick& Pk, PlateTray& Tr, OxCnc& Ox, OxCnc& Grbl2, StrictLock<OxCnc>& oxLock,
	StrictLock<OxCnc>& grbl2Lock, WormFinder& worms, CameraFactory& camFact, int& kg,
	bool verify_drop_over_whole_plate, PlateTracker& plateTracker)
{

	// Get the location of source plate
	int source_row = Tr.getCurrentScriptStepInfo().id.row;
	int source_col = Tr.getCurrentScriptStepInfo().id.col;
	MatIndex source_loc(source_row, source_col);

	// Get the location of destination plate
	int dest_row = stoi(Tr.getCurrentScriptStepInfo().args[0]);
	int dest_col = stoi(Tr.getCurrentScriptStepInfo().args[1]);
	MatIndex dest_loc(dest_row, dest_col);
	Pk.lid_handler.setPlateType(dest_loc, LidHandler::IS_DESTINATION);

	// Get the location of intermediate picking plate
	int inter_row = stoi(Tr.getCurrentScriptStepInfo().args[2]);
	int inter_col = stoi(Tr.getCurrentScriptStepInfo().args[3]);
	MatIndex inter_loc(inter_row, inter_col);

	// Get the number of worms to transfer
	int num_worm_to_pick = stoi(Tr.getCurrentScriptStepInfo().args[4]);


	// Get phenotyping parameters from the argument list
	int sex_type_idx = stoi(Tr.getCurrentScriptStepInfo().args[5]);
	int GFP_type_idx = stoi(Tr.getCurrentScriptStepInfo().args[6]);
	int RFP_type_idx = stoi(Tr.getCurrentScriptStepInfo().args[7]);
	int morphology_type_idx = stoi(Tr.getCurrentScriptStepInfo().args[8]);
	int stage_type_idx = stoi(Tr.getCurrentScriptStepInfo().args[9]);

	// Get strain and generation number from the argument list
	string desired_strain = Tr.getCurrentScriptStepInfo().args[10];
	int genNum = stoi(Tr.getCurrentScriptStepInfo().args[11]);


	// check input index is valid or not
	// TODO: Check whether index of plate is OOB
	if (
		!(sex_type_idx >= 0 && sex_type_idx < DUMMY_COUNT_SEX_TYPE) ||
		!(GFP_type_idx >= 0 && GFP_type_idx < DUMMY_COUNT_GFP_TYPE) ||
		!(RFP_type_idx >= 0 && RFP_type_idx < DUMMY_COUNT_RFP_TYPE) ||
		!(morphology_type_idx >= 0 && morphology_type_idx < DUMMY_COUNT_MORPH_TYPE) ||
		!(stage_type_idx >= 0 && stage_type_idx < DUMMY_COUNT_STAGE_TYPE)
		) {
		cout << "Invalid idexing for the phenotyping option!" << endl;
		return false;
	}


	int num_worms_in_himag = 1; // This is passed as a reference when finding worms, it will contain the number of worms in
						 // the high mag camera, so we know if we need to do an intermediate pick or not.


	// Construct a object holding the phenotyping parameter based on the desired phenotypes
	Phenotype phenotype = Phenotype(
		(sex_types)sex_type_idx,
		(GFP_types)GFP_type_idx,
		(RFP_types)RFP_type_idx,
		(morph_types)morphology_type_idx,
		(stage_types)stage_type_idx
	);
	phenotyping_method_being_used = phenotyping_method(phenotype);

	// dummy tray object for preparing script params later on
	PlateTray dummy_tray(Tr);


	for (int i = 0; i < num_worm_to_pick; i++) {

		cout << "Attempting to find, pick, and drop worm: " << i << endl;
		if (experiment_data.is_open()) experiment_data << "Attempting worm " << i + 1 << " of " << num_worm_to_pick << " from the source plate." << endl;

		// Move to picking plate
		cout << "============= Moving to source ==============" << endl;
		dummy_tray.changeScriptId(source_loc);
		dummy_tray.updateStepInfo();
		dummy_tray.changeScriptArgs(MOVE_TO_SOURCE_DEFAULT); // set move inputs
		executeMoveToPlate(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);

		if (experiment_data.is_open()) experiment_data << "\tAt source plate." << endl;

		// Find and center a worm to pick
		cout << "============= Finding and centering a worm ==============" << endl;
		vector<Phenotype> phenotypes_of_worm_centered;
		int num_tries = 0;
		int max_tries = 30;
		bool success = false;

		// Try multiple time to find and center a worm w/ desired phenotype
		do {
			cout << "Attempting to find and center a worm for picking. Try: " << num_tries + 1 << endl;
			if (experiment_data.is_open())
				experiment_data << "\tAttempt " << num_tries + 1 << " of " << max_tries << " to find and center a worm for picking." << endl;

			dummy_tray.changeScriptArgs(CENTER_WORMS_DEFAULT); // set centerworm inputs
			success = executeCenterWorm(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact, kg, phenotypes_of_worm_centered, num_worms_in_himag);
			num_tries++;
			if (!success) {
				cout << "\tFailed to find and center a worm for picking." << endl;
				if (experiment_data.is_open()) experiment_data << "\tFailed to find and center a worm for picking." << endl;
			}
			cout << "------------------- Num worms found: " << num_worms_in_himag << endl;
		} while (!success && num_tries < max_tries);

		// Give up finding worm
		if (!success) {
			cout << "Failed to select a worm during the execute center worm step." << endl;
			if (experiment_data.is_open()) experiment_data << "\tFailed to select a worm during the center worm step.";
			
			// TODO: Move to target plate
			dummy_tray.changeScriptId(dest_loc);
			dummy_tray.updateStepInfo();
			dummy_tray.changeScriptArgs(MOVE_TO_DESTINATION_DEFAULT); // set move inputs
			executeTestReadBarcode(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact, plateTracker);

			// Write an update to DB
			Plate oldTarget = plateTracker.readCurrentPlate();
			if (oldTarget == INVALID_PLATE) {
				// Write target as new plate
				plateTracker.writeCurrentPlate(desired_strain, { { phenotype, i } }, genNum);
			}
			else {
				// Write an update to target
				auto itAndWasInserted = oldTarget.phenotypes.insert({ phenotype, i });
				// If collision (phenotype exists), incrementally update the worm number for that phenotype
				if (!itAndWasInserted.second)
				{
					oldTarget.phenotypes[phenotype] = i + oldTarget.phenotypes.find(phenotype)->second;
				}
				plateTracker.writeCurrentPlate(desired_strain, oldTarget.phenotypes, genNum);
			}
			
			return false;
		}

		// up to this point a worm w/ desired phenotype has been centered
		if (experiment_data.is_open()) experiment_data << "\tWorm has been selected for picking." << endl;
		cout << "\tWorm has been selected for picking." << endl;


		// Pick the worm
		cout << "============= Picking a worm ==============" << endl;
		dummy_tray.changeScriptArgs(PICK_UP_WORMS_DEFAULT); // set pickworm inputs
		executeSpeedPick(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);

		// If more than one worm in high-mag cam, then pick to intermediate plate before going to destination
		bool int_pick_success = true;
		while (num_worms_in_himag > 1) {
			int_pick_success = performIntermediatePickingStep(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact, kg, num_worms_in_himag, inter_loc, phenotypes_of_worm_centered);
			if (!int_pick_success) break;
		}

		if (!int_pick_success) {
			// We failed the intermediate picking action, so just pick up where we left off on the current plate
			i--; // decrement i so when we loop we are attempting the same worm number as before (because we didn't actually get the worm so we don't want to try for the next worm)
			continue;
		}

		if (experiment_data.is_open()) experiment_data << "\tPicking action complete. Moving to destination plate." << endl;
		cout << "\tPicking action complete. Moving to destination plate." << endl;

		// boost_sleep_ms(1000); // Just to get a quick look at the plate after the pick to see if we think it was successful or not.
		// Move to the destination plate
		cout << "============= Moving to destination ==============" << endl;
		dummy_tray.changeScriptId(dest_loc);
		dummy_tray.updateStepInfo();
		dummy_tray.changeScriptArgs(MOVE_TO_DESTINATION_DEFAULT); // set move inputs
		executeMoveToPlate(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);

		cout << "\tAt destination plate. Attempting to drop the worm." << endl;
		if (experiment_data.is_open()) experiment_data << "\tAt destination plate. Attempting to drop the worm." << endl;


		// Drop the worm at the destination plate
		cout << "============= Dropping worm at destination ==============" << endl;
		//worms.overlayflag = false;

		// set pickworm inputs
		if (verify_drop_over_whole_plate) dummy_tray.changeScriptArgs(DROP_OFF_WORMS_ON_EMPTY_DEFAULT);
		else dummy_tray.changeScriptArgs(DROP_OFF_WORMS_DEFAULT);

		bool verified_drop = executeSpeedPick(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);
		//worms.overlayflag = true;
		if (!verified_drop) { i--; } // We didn't verify the worm drop we will try again for the same type of worm (don't increment when loop)
	}

	// TODO: Move to target plate (may not be needed? at this point camera should be at target plate)
	dummy_tray.changeScriptId(dest_loc);
	dummy_tray.updateStepInfo();
	dummy_tray.changeScriptArgs(MOVE_TO_DESTINATION_DEFAULT); // set move inputs
	executeTestReadBarcode(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact, plateTracker);

	// Write an update to DB
	Plate oldTarget = plateTracker.readCurrentPlate();
	if (oldTarget == INVALID_PLATE) {
		// Write target as new plate
		plateTracker.writeCurrentPlate(desired_strain, { { phenotype, num_worm_to_pick } }, genNum);
	}
	else {
		// Write an update to target
		auto itAndWasInserted = oldTarget.phenotypes.insert({ phenotype, num_worm_to_pick });
		// If collision (phenotype exists), incrementally update the worm number for that phenotype
		if (!itAndWasInserted.second)
		{
			oldTarget.phenotypes[phenotype] = num_worm_to_pick + oldTarget.phenotypes.find(phenotype)->second;
		}
		plateTracker.writeCurrentPlate(desired_strain, oldTarget.phenotypes, genNum);
	}

  // script success upon this point
	return true;

}


/*
executeScreenOnePlate: a mid-level script that inspects phenotypes of certain amount of worms over a single plate.
The mid-level script is called by high-level scripts
Script args:
	id:			row and col of the plate to be screened
	arg0:		number of worms want to screen
	arg1:		is there any specific phenotype want to screen for
					Yes:	get percentage for the wanted phenotype out of the total number of worms screened
							can quit when this plate containing too low percentage phenotype you wanted, to save time.
					No:		check phenotypes of interested until reach number of worms user requested
							can get distribution of different phenotypes.

				If yes, then arg2-6 take the wanted phenotype
				arg2: type of sex wanted
				arg3: type of GFP wanted
				arg4: type of RFP wanted
				arg5: type of morphology wanted
				arg6: type of developmental stage
				arg7: percentage threshold above which the screen can quit

				If No, then arg2-6 take phenotypes you want to check
				arg2: whether check sex
				arg3: whether check GFP
				arg4: whether check RFP
				arg5: whether check morphology
				arg6: whether check developmental stage

    arg9: whether exit after screen
*/
bool executeScreenOnePlate(WormPick &Pk, PlateTray &Tr, OxCnc &Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder& worms, CameraFactory& camFact, int& kg) {

	// Record the important data of the experiment as it runs
	string tFile = "C:\\WormPicker\\recordings\\Experiments\\";
	string date_string = getCurrentDateString();
	string exp_title = "experiment";
	if (global_exp_title != "") {
		exp_title = global_exp_title;
	}
	tFile.append(exp_title); tFile.append("_");
	tFile.append(date_string); tFile.append(".txt");
	experiment_data.open(tFile, std::fstream::app);
	// To delete later



	// Get the location of plate to screen
	int screen_row = Tr.getCurrentScriptStepInfo().id.row;
	int screen_col = Tr.getCurrentScriptStepInfo().id.col;
	MatIndex screen_loc(screen_row, screen_col);


	// Get number of worms to screen
	int num_worm_to_screen = stoi(Tr.getCurrentScriptStepInfo().args[0]);

	// Get whether there is any specific phenotype want to screen for
	bool any_wanted_phenotype = stoi(Tr.getCurrentScriptStepInfo().args[1]);

    // Get whether want to exit program after screening
    bool exit_after_screen = stoi(Tr.getCurrentScriptStepInfo().args[9]);

	// Percentage threshold
	double percent_thresh = -1;

	// If Yes
	if (any_wanted_phenotype) {

		// Get wanted phenotyping parameters from the argument list
		int sex_type_idx = stoi(Tr.getCurrentScriptStepInfo().args[2]);
		int GFP_type_idx = stoi(Tr.getCurrentScriptStepInfo().args[3]);
		int RFP_type_idx = stoi(Tr.getCurrentScriptStepInfo().args[4]);
		int morphology_type_idx = stoi(Tr.getCurrentScriptStepInfo().args[5]);
		int stage_type_idx = stoi(Tr.getCurrentScriptStepInfo().args[6]);

		percent_thresh = stod(Tr.getCurrentScriptStepInfo().args[7]);


		// check input index is valid or not
		// TODO: Check whether index of plate is OOB
		if (
			!(sex_type_idx >= 0 && sex_type_idx < DUMMY_COUNT_SEX_TYPE) ||
			!(GFP_type_idx >= 0 && GFP_type_idx < DUMMY_COUNT_GFP_TYPE) ||
			!(RFP_type_idx >= 0 && RFP_type_idx < DUMMY_COUNT_RFP_TYPE) ||
			!(morphology_type_idx >= 0 && morphology_type_idx < DUMMY_COUNT_MORPH_TYPE) ||
			!(stage_type_idx >= 0 && stage_type_idx < DUMMY_COUNT_STAGE_TYPE) ||
			!(percent_thresh >= 0 && percent_thresh <= 1)
			) {
			cout << "Invalid input arguments!" << endl;
			return false;
		}

		phenotyping_method_being_used = phenotyping_method
		(
			Phenotype(
				(sex_types)sex_type_idx,
				(GFP_types)GFP_type_idx,
				(RFP_types)RFP_type_idx,
				(morph_types)morphology_type_idx,
				(stage_types)stage_type_idx
			)
		);
	}
	else {

		// Get wanted phenotyping parameters from the argument list
		bool check_sex = stoi(Tr.getCurrentScriptStepInfo().args[2]) != 0;
		bool check_GFP = stoi(Tr.getCurrentScriptStepInfo().args[3]) != 0;
		bool check_RFP = stoi(Tr.getCurrentScriptStepInfo().args[4]) != 0;
		bool check_morphology = stoi(Tr.getCurrentScriptStepInfo().args[5]) != 0;
		bool check_stage = stoi(Tr.getCurrentScriptStepInfo().args[6]) != 0;

		phenotyping_method_being_used = phenotyping_method(
			check_sex,
			check_GFP,
			check_RFP,
			check_morphology,
			check_stage
		);
	}

	// Run the experiment!
	PlateTray dummy_tray(Tr);

	AnthonysTimer total_timer;
	int num_worms_in_himag = 1; // This is passed as a reference when finding worms, it will contain the number of worms in
								// the high mag camera.


	cout << "============= Moving to source ==============" << endl;
	dummy_tray.changeScriptId(screen_loc);
	dummy_tray.updateStepInfo();
	dummy_tray.changeScriptArgs(MOVE_TO_SCREEN_PLATE_DEFAULT); // set move inputs
	executeMoveToPlate(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);

	if (experiment_data.is_open()) experiment_data << "\tAt source plate (" << screen_loc.row << ", " << screen_loc.col << ")" << endl;


	// Find and center a worm to phenotype
	cout << "============= Finding and centering a worm ==============" << endl;
	vector<Phenotype> phenotypes_of_worm_on_the_plate;
	int num_tries = 0;
	int max_tries = num_worm_to_screen * 3;
	int num_worms_centered = 0;

	int num_successful_phenotypes = 0;
	int num_unsuccess_phenotypes = 0;

	do
	{
		cout << "Attempting to find and center a worm. Try: " << num_tries + 1 << " of " << max_tries << endl;
		cout << num_worms_centered << " of " << num_worm_to_screen << " worms centered so far." << endl;
		if (experiment_data.is_open()) experiment_data << "\tAttempt " << num_tries + 1 << " of " << max_tries << " to find and center a worm for phenotyping." << endl;


		dummy_tray.changeScriptArgs({ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }); // set centerworm inputs
		vector<Phenotype> phenotypes_of_worm_centered;
		bool success = executeCenterWorm(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact, kg, phenotypes_of_worm_centered, num_worms_in_himag);

		if (num_worms_in_himag != 0) // If we don't even get a worm to look at then we won't count it towards our trials
		{
			// Append the phenotype to the list
			for (int i = 0; i < phenotypes_of_worm_centered.size(); i++) {
				phenotypes_of_worm_on_the_plate.push_back(phenotypes_of_worm_centered[i]);
			}
			num_worms_centered++;
		}

		if (any_wanted_phenotype){
			if (success) num_successful_phenotypes++;
			if (!success && num_worms_in_himag != 0) num_unsuccess_phenotypes++;
		}

		num_tries++;

		if (any_wanted_phenotype && num_unsuccess_phenotypes >= double(num_worm_to_screen) * (1 - percent_thresh) + 1) {
			cout << "Above expected threshold, Quit the screening..." << endl;
			break;
		}

	} while (num_tries < max_tries && num_worms_centered < num_worm_to_screen);

	// TODO: check any_wanted_phenotype is true

	// Write the phenotypes to file
	experiment_data << "\tDone screening plate for worms." << endl;
	if (experiment_data.is_open()) {

		if (!any_wanted_phenotype) {
			experiment_data << "Screen result of Plate (" << to_string(screen_loc.row) << ", " << to_string(screen_loc.col) << ")\n";
			for (int i = 0; i < phenotypes_of_worm_on_the_plate.size(); i++) {
				phenotypes_of_worm_on_the_plate[i].WriteToFile(experiment_data);
			}
		}
		else {
			experiment_data << "\nScreen results:\nPlate\t\tRatio\t\tPercentage\n";
			experiment_data << "(" << to_string(screen_loc.row) << ", " << to_string(screen_loc.col) << ")\t\t" 
				<< to_string(num_successful_phenotypes) << "\\" << to_string(num_worms_centered) << "\t\t" 
				<< to_string(((double)num_successful_phenotypes / num_worms_centered)) << "\n";
		}
	}

	// script success upon this point

	Pk.lid_handler.dropAllLids(Tr, Ox);

	//experiment_data << "Experiment over! Total time: " << total_timer.getElapsedTime() << endl;
	experiment_data.close();

	//TODO: Delete later
    if (exit_after_screen) { exit(EXIT_SUCCESS); }

	return true;

}


// Will return a random offset within the desired bounds. You can set the bounds by pointing the move_offset_region pointer to a rect
// before calling this function. Otherwise the default offset bounds will be used.
bool executeRandomDropOffset(OxCnc &Ox, double& x_offset, double& y_offset) {
	srand((unsigned)time(NULL));
	
	// Default offset will generate an offset between -5 and 10 mm in x
	// and between -10 and +10mm in y
	Rect default_offset(-5, -10, 15, 20);
	if (move_offset_region == nullptr) {
		move_offset_region = &default_offset;
	}

	int x_min = (move_offset_region->x) * 10;
	int y_min = (move_offset_region->y) * 10;
	int x_max = (move_offset_region->width) * 10;
	int y_max = (move_offset_region->height) * 10;

	// Generate x and y offset
	x_offset = (rand() % x_max + x_min) / double(10); 
	y_offset = (rand() % y_max + y_min) / double(10);

	cout << "Random offset for drop: " << x_offset << ", " << y_offset << endl;

	// Move the stage according to the offset
	//Ox.goToRelative(x_offset, y_offset, 0, 1, true);
	move_offset_region = nullptr;
	return true;
}

bool executeSpeedPick(WormPick &Pk, PlateTray& Tr, OxCnc &Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder &worms, CameraFactory& camFact, int* num_expected_worms) {
	int dummy_num_worms = 1;
	if (!num_expected_worms) {
		num_expected_worms = &dummy_num_worms;
	}
	// Do pick action
	Pk.in_pick_action = true;
	int success = true;
	int num_worms_found = 0;
	int num_tries = 0;
	int max_tries = *num_expected_worms == 1 ? 3 : 3; // If we're only dropping 1 worm then try at most twice, otherwise we can try 3 times.
														// Change to try 3 times anyway
	do {

		//*num_expected_worms -= num_worms_found;
		cout << "Attempting picking action. Number of expected worms: " << *num_expected_worms << endl;
		num_worms_found = Pk.pickOrDropOneWorm(Ox, Grbl2, worms,
			stoi(Tr.getCurrentScriptStepInfo().args[0]), // Dig amount, dxl units
			stoi(Tr.getCurrentScriptStepInfo().args[1]), // Sweep amount, dxl units
			stod(Tr.getCurrentScriptStepInfo().args[2]), // Pause time (seconds)
			stoi(Tr.getCurrentScriptStepInfo().args[3]) != 0, // Restore height after - e.g. don't learn touch height from this picking operation
			stoi(Tr.getCurrentScriptStepInfo().args[4]) != 0, // Do centering during the pick
			(pick_mode) stoi(Tr.getCurrentScriptStepInfo().args[5]), // See below for options
			stoi(Tr.getCurrentScriptStepInfo().args[6]) != 0, // Whether verify worm drop over the entire plate (usually true for drop worm on empty plate)
			getPlateIdKey(Tr), // So the pick knows what plate it is at 
			*num_expected_worms); // So we know how many worms to look for during drop verification
		num_tries++;
		if (stoi(Tr.getCurrentScriptStepInfo().args[1]) < 0) {
			*num_expected_worms -= num_worms_found;
			success = num_worms_found >= *num_expected_worms;
			if (!success) {
				if (experiment_data.is_open()) {
					experiment_data << "\t\tDrop attempt " << num_tries << " of " << max_tries << "... Failed." << num_worms_found << " of "<< *num_expected_worms + num_worms_found << " found." << endl;
				}
				cout << "Failed to verify worm drop. " << num_worms_found << " of " << *num_expected_worms + num_worms_found << " found." << endl;

				if (num_worms_found > 0) {
					// If we're dropping multiple worms but not all of them crawled away then we can move to a new spot on the plate (so we don't accidentally pick back up the worm we dropped)
					Tr.changeScriptArgs(MOVE_TO_DESTINATION_DEFAULT); // set move inputs
					executeMoveToPlate(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);
					Tr.changeScriptArgs(DROP_OFF_WORMS_DEFAULT); // set pickworm inputs
				}
			}
			else {
				if (experiment_data.is_open()) {
					experiment_data << "\t\tDrop attempt " << num_tries << " of " << max_tries << "... Success! "<< num_worms_found << " of " << *num_expected_worms + num_worms_found << " found. Verfied with " << worms.recent_verification_technique << endl;
				}
				cout << "Verified worm drop! " << num_worms_found << " of " << *num_expected_worms + num_worms_found << " found." << endl;
			}
		}
		
	} while (!success && stoi(Tr.getCurrentScriptStepInfo().args[1]) < 0 && num_tries < max_tries); // When dropping a worm, we must verify the drop success. So try a few times until we succeed.
/*
	AUTO = 0 = automatically find worm before picking
	MANUAL = 1 = user clicks on worm before picking
	DROP = 2 = don't find a worm, but look for it after (buggy as of 10/23/2019)
	NONE = 3 = Just pick, e.g. for coating.
*/
	cout << "Finished pickordroponeworm" << endl;
	Pk.in_pick_action = false;

	return success;




}

// DEPRECATED version of executeCenterWorm
// This version attempts to find all the worms 1 time, then loop through and center them one by one until we find a matching phenotype.
// It is buggy though, because for some reason when we attempt to center the worm we miss. I feel this is the best way to do it, but I'm having a hard time to get it to work consistently.
// Arg0 == 1 : Only center to worms within wormfinder's pickable region
// Arg1 == 1 : Immediatley position the pick to the start position once we've found a worm
//bool executeCenterWorm(WormPick &Pk, PlateTray &Tr, OxCnc &Ox, OxCnc &Grbl2,
//						 StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock,
//						 WormFinder& worms, CameraFactory& camFact, int& kg, Phenotype& worm_returned)
//{
//
//	bool worm_selected = false;
//	int num_worms = 0;
//
//	worms.worm_finding_mode = worms.FIND_ALL;
//
//	// Wait for the worm finder thread to finish finding the worms
//	// While we're waiting we can check to see if the pick is done heating
//	AnthonysTimer finding_timer;
//	while (worms.worm_finding_mode != worms.WAIT) {
//
//		boost_sleep_ms(50);
//		boost_interrupt_point();
//	}
//
//	// Sort the worms based on distance from High Mag FOV center.
//	compWormDist compObject; compObject.ref_pt = worms.high_mag_fov_center_loc;
//	sort(worms.all_worms.begin(), worms.all_worms.end(), compObject);
//
//	worms.worm_finding_mode = worms.TRACK_ALL;
//
//	cout << "Worms list size: " << worms.all_worms.size() << endl;
//
//	if (worms.all_worms.size() == 0) return false;
//
//	// Move Grbl right above the focus height - TODO: Move this into readyPickToStartPosition so we do it in parallel
//	//Grbl2.goToAbsolute(Grbl2.getX(), Grbl2.getY(), Pk.getPickupHeight() - 2, true, 0);
//	
//
//	// Optionally move the pick to the start position while we select and center a worm.
//	if (Tr.getCurrentScriptStepInfo().args[1] == 1)
//		Pk.readyPickToStartPositionThread(Grbl2, Pk);
//	//else
//		//Pk.readyPickToStartPosition(Grbl2);
//
//	vector<int> pickable_worm_idxs;
//
//	// Optionally, We don't want to target a worm that is too near the edge for picking
//	// So we'll neglect worms outside of the "pickable region"
//	if (Tr.getCurrentScriptStepInfo().args[0] == 1) {
//		// Loop through the worms to find one that is within the pickable region
//		worms.getPickableWormIdxs(pickable_worm_idxs);
//
//		if (pickable_worm_idxs.size() == 0) {
//			cout << "Could not find any worms within the pickable region." << endl;
//			return false;
//		}
//	}
//	else {
//		for (int i = 0; i < worms.all_worms.size(); i++) {
//			pickable_worm_idxs.push_back(i);
//		}
//		// If we don't want the pre-defined pickable region then just set the tracked worm to the first in the list
//		//worms.setTrackedWorm(&worms.all_worms[0]);
//	}
//
//	// Now loop through all the pickable worms, center them one by one, and phenotype them, until we get one matching our desired phenotypes.
//	bool desired_worm_found = true;
//	for (int i = 0; i < pickable_worm_idxs.size(); i++) {
//		Worm& tracked_worm = worms.all_worms[pickable_worm_idxs[i]];
//		if (!tracked_worm.is_visible || tracked_worm.is_lost) {
//			cout << "Current worm is not visible or was lost. Trying next worm." << endl;
//			continue;
//		}
//		
//		worms.setTrackedWorm(&tracked_worm);
//		cout << "Waiting a few seconds to show what worms have been found and tracked..." << endl;
//		boost_sleep_ms(1000);
//		//Worm& tracked_worm = *(worms.getTrackedWorm());
//		
//		//cout << "centroid worm: " << tracked_worm.getWormCentroidGlobal() << endl;
//		//cout << ", Waiting a few seconds before centering the tracked worm..." << endl;
//		//boost_sleep_ms(7000);
//
//		///////////////////////////
//		//// Center the worm in the high mag field of view
//		//double dx = (tracked_worm.getWormCentroidGlobal().x - worms.high_mag_fov_center_loc.x) / Pk.dydOx;
//		//double dy = (tracked_worm.getWormCentroidGlobal().y - worms.high_mag_fov_center_loc.y) / Pk.dydOx;
//
//		////cout << "dx: " << dx << ", dy: " << dy << endl;
//
//		//int old_worm_mode = worms.worm_finding_mode;
//		//worms.worm_finding_mode = worms.WAIT;
//		//boost_sleep_ms(100);
//
//		//cout << "Moving to tracked worm " << i + 1 << " of " << pickable_worm_idxs.size() << endl;
//		//Ox.goToRelative(dx, dy, 0, 1.0, true);
//		//worms.updateWormsAfterStageMove(dx, dy, Pk.dydOx);
//		////cout << "Targeted worm current pos: " << tracked_worm.getWormCentroidGlobal().x << ", " << tracked_worm.getWormCentroidGlobal().y << endl;
//		//boost_sleep_ms(500);
//
//		//worms.worm_finding_mode = old_worm_mode;
//		//////////////////////////////////////
//
//		worm_selected = centerTrackedWormToPoint(worms, Ox, worms.high_mag_fov_center_loc);
//		if (!worm_selected) {
//			cout << "No worm could be selected in centerworm." << endl;
//			return false;
//		}
//		else {
//			// for now we are just going to return herm worms. TODO: We will need to phenotype in the future...
//
//			// DO PHENOTYPING HERE... Need to look at current_phenotyping_desired and run executePhenotyping with it. Will need to set the current phenotyping in the high level script each time we start on a new plate. 
//			
//			desired_worm_found = setAndRunPhenotyping(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact, num_worms);
//
//			if (desired_worm_found) {
//				cout << "Worm with desired phenotyping has been centered!" << endl;
//				worm_returned = current_phenotyping_method.getWantedPhenotype();
//				break;
//			}
//			else {
//				cout << "The centered worm did not match the desired phenotyping. Trying the next worm." << endl;
//			}
//
//			//worm_returned = worms.HERM;
//		}
//
//		//bool want_fluo = true; // TODO : Move want_fluo to be a script action input
//
//		//Mat brightfield_image;
//		//Mat brightfield_image_color;
//
//
//		// Get a brightfield high mag image to segment worm
//		//worms.img_high_mag.copyTo(brightfield_image);
//		//worms.img_high_mag_color.copyTo(brightfield_image_color);
//
//		//tracked_worm.segmentWorm(brightfield_image);
//		//tracked_worm.drawSegmentation(brightfield_image_color, false);
//		//imshow("Brightfield color w/ seg", brightfield_image);
//		//waitKey(1);
//
//
//		// Camera is centered on worm. Check for fluorescence if necessary.
//		//if (want_fluo && worm_selected) {
//		//	Mat brightfield_image;
//		//	Mat fluo_image;
//		//	Mat fluo_color;
//
//		//	// Get a brightfield high mag image to segment worm
//		//	//worms.img_high_mag.copyTo(brightfield_image);
//		//		
//		//	//Switch to fluo imaging mode and grab an image
//		//	cout << "testing out the fluo imaging mode." << endl;
//		//	long long expo_time = 100;
//		//	// Switch to fluorescence imaging mode
//		//	ImagingMode(1, expo_time, Grbl2, camFact);
//
//		//	// Wait enough time so the fluo image is synchronized with the fluo imaging mode.
//		//	boost_sleep_ms(340);
//		//	worms.img_high_mag.copyTo(fluo_image);
//		//	//worms.img_high_mag_color.copyTo(fluo_color);
//
//		//	// Switch back to brightfield imaging mode
//		//	ImagingMode(0, 2.5, Grbl2, camFact); // TO DO: get the original exposure time from camera
//
//		//	//imshow("brightfield img", brightfield_image);
//		//	//waitKey(1);
//		//	//
//		///*	imshow("Fluo image", fluo_image);
//		//	waitKey(1);*/
//
//		//	//
//		//	//tracked_worm.drawSegmentation(fluo_color, false);
//		//	//imshow("Fluo color w/ seg", fluo_color);
//		//	//waitKey(1);
//
//		//	// Brightfield-to-fluo transition time is too slow to use this method
//		//	//tracked_worm.segmentWorm(brightfield_image);
//		//	//tracked_worm.fluoProfile(fluo_image);
//
//		//	// Check for ANY fluo objects
//		//	worm_selected = tracked_worm.segmentFluo(fluo_image, 26);
//
//		//	if (worm_selected)
//		//		worm_returned = worms.FLUO_TYPE;
//		//	else
//		//		worm_returned = worms.NON_FLUO_TYPE;
//		//
//
//		//	double minVal;
//		//	double maxVal;
//		//	Point minLoc;
//		//	Point maxLoc;
//
//		//	minMaxLoc(fluo_image, &minVal, &maxVal, &minLoc, &maxLoc);
//
//		//	vector<int> params{ CV_IMWRITE_PNG_COMPRESSION, 0 };
//		//	string fout_frames_final = fluo_img_dir;
//		//	fout_frames_final.append(getCurrentTimeString(false, true)); fout_frames_final.append("_");
//		//	fout_frames_final.append(to_string(worm_returned)); fout_frames_final.append("_"); 
//		//	fout_frames_final.append(to_string(minVal)); fout_frames_final.append("_"); 
//		//	fout_frames_final.append(to_string(maxVal)); fout_frames_final.append(".png");
//		//	imwrite(fout_frames_final, fluo_image, params);
//		//}
//
//		// For now we want to sort each worm to another plate regardless of phenotype. SO we will return true so we can sort each worm
//		//return true;
//
//		/*
//			Our phenotyping module is not ready for automatic sex determination.
//			The code below request whether the worm is Herm or Male from the user
//			The code here is written to do strain crossing with user supervision, generating results for 2021 worm conference
//		*/
//
//		//////////////////////// CODE BELOW FOR USER TO MANUALLY INPUT THE SEX OF THE WORM WE FOUND
//		// Request the input from user
//
//		//string sex_input_fragment = string("H");
//		//cout << "Please specify the sex of the worm in the high-mag FOV:    H (Herm.)  -or-  M (Male)...\n\n";
//		////getline(cin, sex_input_fragment);
//
//
//		//if (sex_input_fragment == string("H") || sex_input_fragment == string("h")) {
//		//	worm_returned = worms.HERM;
//		//}
//		//else if (sex_input_fragment == string("M") || sex_input_fragment == string("m")) {
//		//	worm_returned = worms.MALE;
//		//}
//		//else {
//		//	cout << "Invalid or empty input received! Abort the phenotyping process..." << endl;
//		//	worm_returned = worms.UNKNOWN_SEX;
//		//}
//
//		////worm_returned = worms.NON_FLUO_TYPE;
//		//cout << "Worm returned from center worm: " << sex_input_fragment << "	enum = " << worm_returned << endl;
//		//// boost_sleep_ms(5000);
//
//		//// Recenter the worm since the in case the user take too long to give a decision
//		//// during which the worm can crawl away
//		//worm_selected = centerTrackedWormToPoint(worms, Ox, worms.high_mag_fov_center_loc);
//
//
//		//if (!worm_selected) {
//		//	worm_returned = worms.NO_WORM;
//		//	cout << "No worm could be selected in centerworm." << endl;
//		//	return false;
//		//}
//
//
//	}
//
//	Ox.abortJog();
//
//	return desired_worm_found;
//
//}

// This version will move to a random offset within the plate, find all worms, center the one closest to the high mag, and check if its a matching phenotype.
// If its not a match then we do the whole process over again until we find a match or until we have tried a set number of times with no match.
// Arg0 == 1 : Only center to worms within wormfinder's pickable region
// Arg1 == 1 : Immediatley position the pick to the start position once we've found a worm
bool executeCenterWorm(WormPick &Pk, PlateTray &Tr, OxCnc &Ox, OxCnc &Grbl2,
	StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock,
	WormFinder& worms, CameraFactory& camFact, int& kg, std::vector<Phenotype>& phenotypes_found, int& num_worms)
{
	bool desired_worm_found = false;
	num_worms = 0;
	int num_tries = 0;
	int max_tries = 1;
	PickableRegion original_pickable_region = worms.pickable_region;
	bool only_pick_within_region = stoi(Tr.getCurrentScriptStepInfo().args[0]) == 1;
	bool move_pick_to_start = stoi(Tr.getCurrentScriptStepInfo().args[1]) == 1;

	do {
		if (experiment_data.is_open()) {
			experiment_data << "\t\tCentering worm attempt...";
		}
		cout << "	##################### Finding all worms and centering one, try #" << num_tries + 1<< " ########################" << endl;

		bool worm_selected = false;

		// First move to a random offset within the plate.
		//PlateTray dummy_tray(Tr);
		//dummy_tray.changeScriptArgs({ 1, 0, 1, 0, 0, 0, 0, 0, 0, 0 }); // set move inputs
		Tr.changeScriptArgs({ "1", "0", "1", "0", "0", "0", "0", "0", "0", "0" }); // set move inputs
		//executeMoveToPlate(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);
		executeMoveToPlate(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);

		//worms.image_timer.setToZero();
		worms.worm_finding_mode = worms.FIND_ALL;
		worms.image_timer.setToZero();

		// Wait for the worm finder thread to finish finding the worms
		// While we're waiting we can check to see if the pick is done heating
		//AnthonysTimer finding_timer;
		while (worms.worm_finding_mode != worms.WAIT) {

			boost_sleep_ms(50);
			boost_interrupt_point();
		}

		cout << "Worms list size: " << worms.all_worms.size() << endl;

		if (worms.all_worms.size() == 0) {
			num_tries++;
			if (experiment_data.is_open()) {
				experiment_data << "No worms were found in Find All mode." << endl;
			}
			cout << "	##################### No worms found, Done with try #" << num_tries << " ########################" << endl;
			continue;
		}

		// Sort the worms based on distance from High Mag FOV center.
		compWormDist compObject; compObject.ref_pt = worms.high_mag_fov_center_loc;
		sort(worms.all_worms.begin(), worms.all_worms.end(), compObject);

		//worms.worm_finding_mode = worms.TRACK_ALL;
		vector<int> pickable_worm_idxs;

		// Optionally, We don't want to target a worm that is too near the edge for picking
		// So we'll neglect worms outside of the "pickable region"
		if (only_pick_within_region) {
			// Update the position of the pickable region based on the offset we moved to.
			//worms.updatePickableRegion(Ox.getXYZ(), Tr.getCurrentPosition(), Pk.dydOx);

			cout << "New pickable region after offset: " << worms.pickable_region.pickable_center << endl;
			//waitKey(0);

			worms.getPickableWormIdxs(pickable_worm_idxs);

			if (pickable_worm_idxs.size() == 0) {
				cout << "Could not find any worms within the pickable region." << endl;
				worms.pickable_region = original_pickable_region;
				num_tries++;
				cout << "	##################### No worms in pickable region, Done with try #" << num_tries << " ########################" << endl;
				if (experiment_data.is_open()) {
					experiment_data << "No worms were found in the pickable region." << endl;
				}
				continue;
			}
		}
		else {
			for (int i = 0; i < worms.all_worms.size(); i++) {
				pickable_worm_idxs.push_back(i);
			}
			// If we don't want the pre-defined pickable region then just set the tracked worm to the first in the list
			//worms.setTrackedWorm(&worms.all_worms[0]);
		}

		// Now center the tracked worm and phenotype it. If it is a match then return it, otherwise we'll do the whole process again.
		Worm& tracked_worm = worms.all_worms[0]; // We want to track the first worm in the list because its the closest to the high mag FOV
		worms.setTrackedWorm(&tracked_worm);
		worms.worm_finding_mode = worms.TRACK_ONE;
		//cout << "Waiting a few seconds to show what worm has been tracked..." << endl;
		//boost_sleep_ms(1000);
		//Worm& tracked_worm = *(worms.getTrackedWorm());

		//cout << "centroid worm: " << tracked_worm.getWormCentroidGlobal() << endl;
		//cout << ", Waiting a few seconds before centering the tracked worm..." << endl;
		//boost_sleep_ms(7000);

		worm_selected = centerTrackedWormInHighMag(Ox, worms, Pk, Tr);
		if (!worm_selected) {
			worms.pickable_region = original_pickable_region;
			num_tries++;
			cout << "	##################### Couldn't center worm, Done with try #" << num_tries << " ########################" << endl;
			if (experiment_data.is_open()) {
				experiment_data << "Failed to center the tracked worm." << endl;
			}
			continue;
		}
		else {
			
			bool focus_success = executeAutoFocus(Ox, worms, camFact, Tr);
			if (!focus_success) {
				cout << "Laser focusing failed." << endl;
				if (experiment_data.is_open()) {
					experiment_data << "Laser focusing failed." << endl;
				}
				continue;
			}

			// Re-center the worm because it can crawl away during auto focus
			centerTrackedWormInHighMag(Ox, worms, Pk, Tr);

			// DO PHENOTYPING HERE... Need to look at current_phenotyping_desired and run executePhenotyping with it. Will need to set the current phenotyping in the high level script each time we start on a new plate. 
			cout << "Phenotyping..." << endl;

			// desired_worm_found = setAndRunPhenotyping(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact, num_worms);

			Tr.changeScriptArgs({ 
				to_string((int) phenotyping_method_being_used.check_sex),
				to_string((int) phenotyping_method_being_used.check_GFP),
				to_string((int) phenotyping_method_being_used.check_RFP),
				to_string((int) phenotyping_method_being_used.check_morphology),
				to_string((int) phenotyping_method_being_used.check_stage),
				"0", "0", "0", "1", "1" });

			desired_worm_found = executePhenotypeV2(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact, num_worms, phenotypes_found, true);

            // Clear worm list and reset the finding mode to wait
            worms.worm_finding_mode = worms.WAIT;
            worms.all_worms.clear();

			// Do we need to quickly try to center the worm one last time in case it crawled while we were phenotyping? We do try to center again before picking so I'm not sure if this would help.
			/*boost_sleep_ms(150);
			centerTrackedWormInHighMag(Ox, worms, Pk, Tr, false);*/

			// If expected phenotype is not empty
			if (!phenotyping_method_being_used.getWantedPhenotype().is_empty()) {

				if (desired_worm_found) {
					cout << "Worm with desired phenotyping has been centered!" << endl;
					//worm_returned = current_phenotyping_method.getWantedPhenotype();
				}
				else {
					cout << "The centered worm did not match the desired phenotyping. Trying the next worm." << endl;
				}

				if (experiment_data.is_open()) {

					if (desired_worm_found) {
						experiment_data << "\t\t\tA worm matches the desired phenotype!" << endl;
					}
					else {
						experiment_data << "\t\t\tNo worms match the desired phenotype." << endl;
					}
				}
			
			}

			if (experiment_data.is_open()) {
				if (num_worms > 1) {
					experiment_data << num_worms << " worms centered!" << endl;
				}
				else {
					experiment_data <<
						"Worm centered! Gender: " << tracked_worm.current_phenotype.SEX_type <<
						", GFP: " << tracked_worm.current_phenotype.GFP_type <<
						", RFP: " << tracked_worm.current_phenotype.RFP_type <<
						", Morphology: " << tracked_worm.current_phenotype.MORPH_type << endl;
				}
			}
		}
		num_tries++;
		cout << "	##################### Done with try #" << num_tries << " ########################" << endl;
		//boost_sleep_ms(5000);
	}while (!desired_worm_found && num_tries < max_tries);

	// Optionally move the pick to the start position while we select and center a worm.
	if (move_pick_to_start)
		Pk.readyPickToStartPositionThread(Grbl2, Pk);
	//else
		//Pk.readyPickToStartPosition(Grbl2);

	Ox.abortJog();

	worms.pickable_region = original_pickable_region;
	return desired_worm_found;
}

bool executeTestDyDOxCalib(WormPick &Pk, const PlateTray& Tr, OxCnc& Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder& worms, CameraFactory& camFact) {
	PlateTray dummy_tray(Tr);

	// plate coordinates of the first source plate
	int source1_row = Tr.getCurrentScriptStepInfo().id.row;
	int source1_col = Tr.getCurrentScriptStepInfo().id.col;

	int pix_to_move = stoi(Tr.getCurrentScriptStepInfo().args[0]);
	
	// Move to the plate
	dummy_tray.changeScriptArgs({ "1", "0", "0", "0", "0", "0", "0", "0", "0", "0" }); // set move inputs
	executeMoveToPlate(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);

	boost_sleep_ms(2000);

	double dx = (pix_to_move) / Pk.dydOx;
	double dy = (pix_to_move) / Pk.dydOx;
	Worm w;
	worms.setTrackedWorm(&w);
	worms.all_worms.push_back(w);
	w.setGlbCentroidAndRoi(Point(500, 500), 40, worms.imggray.size());
	worms.worm_finding_mode = worms.TRACK_ONE;
	
	// Move left and right a few times.
	for (int i = 0; i < 4; i++) {
		int multiplier = i % 2 == 0 ? -1 : 1;
		Point centroid(worms.high_mag_fov_center_loc.x - (pix_to_move * multiplier), worms.high_mag_fov_center_loc.y);
		w.setGlbCentroidAndRoi(centroid, 40, worms.imggray.size());
		boost_sleep_ms(2000);
		//Mat before(worms.img_low_mag_color);
		//w.drawWormPosition(before, worms.imggray.size(), colors::bb);
		boost_sleep_ms(100);
		Ox.goToRelative( -1 *multiplier*dx, 0, 0, 1.0, true);
		boost_sleep_ms(100);
		w.setGlbCentroidAndRoi(worms.high_mag_fov_center_loc, 40, worms.imggray.size());
		//Mat after(worms.img_low_mag_color);
		//w.drawWormPosition(after, worms.imggray.size(), colors::bb);
		/*imshow("horizontal b", before);
		imshow("horizontal a", after);
		waitKey(0);*/
	}

	// Move up and down a few times.
	for (int i = 0; i < 4; i++) {
		int multiplier = i % 2 == 0 ? -1 : 1;
		Point centroid(worms.high_mag_fov_center_loc.x, worms.high_mag_fov_center_loc.y - (pix_to_move * multiplier));
		w.setGlbCentroidAndRoi(centroid, 40, worms.imggray.size());
		boost_sleep_ms(2000);
		Ox.goToRelative(0, -1 * multiplier * dy, 0, 1.0, true);
	}

	// Move diagonally a few times.
	for (int i = 0; i < 4; i++) {
		int multiplier = i % 2 == 0 ? -1 : 1;
		Point centroid(worms.high_mag_fov_center_loc.x - (pix_to_move * multiplier), worms.high_mag_fov_center_loc.y - (pix_to_move * multiplier));
		w.setGlbCentroidAndRoi(centroid, 40, worms.imggray.size());
		boost_sleep_ms(2000);
		Ox.goToRelative(-1 * multiplier*dx, -1 * multiplier*dy, 0, 1.0, true);
	}

	worms.worm_finding_mode = worms.WAIT;
	return false;
}

/*
Attempt to phenotype any worms in the high mag image and check them against the desired phenotype. 
Note that the num_worms parameter can be used to know how many worms are in the high mag image and determine whether or not you would
like to perform an intermediate pick.
*/
bool executePhenotype(WormPick &Pk, const PlateTray& Tr, OxCnc& Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder& worms, CameraFactory& camFact, int& num_worms, vector<Phenotype>& phenotypes_inferred) {
	worms.unknown_pheno_in_high_mag = false;

	// args[0] whether check sex
	// 0: No sex phenotyping
	// 1: Want sex phenotyping
	bool check_sex = stoi(Tr.getCurrentScriptStepInfo().args[0]) != 0;

	// args[1] whether check GFP
	// 0: No GFP phenotyping
	// 1: Want GFP phenotyping
	bool check_GFP = stoi(Tr.getCurrentScriptStepInfo().args[1]) != 0;
	
	// args[8] the number of GFP spots expected on a single worm (obviously only needed if you're searching for fluo worms)
	int num_GFP_spot_per_worm = stoi(Tr.getCurrentScriptStepInfo().args[8]);

	// args[2] whether check RFP
	// 0: No RFP phenotyping
	// 1: Want RFP phenotyping
	bool check_RFP = stoi(Tr.getCurrentScriptStepInfo().args[2]) != 0;

	// args[9] the number of RFP spots expected on a single worm (obviously only needed if you're searching for fluo worms)
	int num_RFP_spot_per_worm = stoi(Tr.getCurrentScriptStepInfo().args[9]);

	// args[3] whether check morphology
	// 0: No morphology phenotyping
	// 1: Want morphology phenotyping
	bool check_morph = stoi(Tr.getCurrentScriptStepInfo().args[3]) != 0;

	// args[4] whether check stage
	// 0: No stage phenotyping
	// 1: Want stage phenotyping
	bool check_stage = stoi(Tr.getCurrentScriptStepInfo().args[4]) != 0;

	// Grab the worm image
	Mat worm_img; 
	worms.img_high_mag_color.copyTo(worm_img);

	Worm& tracked_worm = *(worms.getTrackedWorm());

	// Feed the image to mask-rcnn server and get results.
	cout << "Sending image to Python server" << endl;
	maskRCNN_results results = maskRcnnRemoteInference(worm_img);
	cout << "Results received. " << endl;
	
	// Get the number of worms in the returned mask
	num_worms = results.num_worms;
	if (num_worms > 1) {
		// If we have multiple worms in the image save it for inspection to double check
		/*Mat debug_mask;
		Mat debug_img;
		worms.img_high_mag_color.copyTo(debug_img);
		results.worms_mask.copyTo(debug_mask);
		debug_mask *= 35;*/
		/*string datetime_string = getCurrentTimeString(false, true);
		string verif_img_fpath = "C:\\WormPicker\\recordings\\verification_images\\high_mag_worm_count\\" + datetime_string + "_" + to_string(num_worms) + "worms_img.png";
		string verif_mask_fpath = "C:\\WormPicker\\recordings\\verification_images\\high_mag_worm_count\\" + datetime_string + "_" + to_string(num_worms) + "worms_mask.png";
		vector<int> params{ CV_IMWRITE_PNG_COMPRESSION, 0 };
		imwrite(verif_img_fpath, debug_img, params);
		imwrite(verif_mask_fpath, debug_mask, params);*/
	}

	if (num_worms == 0) {
		cout << "Mask RCNN network did not find any worms in the high mag image! Exiting phenotyping." << endl;
		return false;
	}

	if (!check_sex && !check_GFP && !check_RFP && !check_morph && !check_stage)
		return true; // No phenotyping desired.

	//if (num_worms > 1) {
	//	// We have a clump of worms so we will just do an intermediate pick without phenotyping so that we can more easily determine what is in the clump.
	//	cout << "Found a clump of " << num_worms << " worms. Returning phenotype success so we can perform an intermediate pick to determing what is in the clump." << endl;
	//	experiment_data << "Found a clump of " << num_worms << " worms. Returning phenotype success so we can perform an intermediate pick to determing what is in the clump." << endl;
	//	return true;
	//}

	
	if (check_GFP) {
		// Load parameters for intensity and area threshold for phenotyping
		json GFP_phenotype_config = Config::getInstance().getConfigTree("phenotyping")["GFP"];
		int intensity_threshold = GFP_phenotype_config["intensity"].get<int>();
		int area_threshold = GFP_phenotype_config["area"].get<int>();


		Mat GFP_img;

		int old_worm_finding_mode = worms.worm_finding_mode;
		worms.worm_finding_mode = worms.WAIT;
		// Enable fluorescence imaging
		ImagingMode(GFP_IMAGING, Grbl2, camFact);

		// Get the fluorescence frame
		worms.img_high_mag.copyTo(GFP_img);

		// Turn back to brightfield imaging
		ImagingMode(BRIGHTFIELD, Grbl2, camFact);
		worms.worm_finding_mode = old_worm_finding_mode;

		if (num_GFP_spot_per_worm == 0) {
			cout << "No value given for the number of expected GFP spots per worm. Using default of 1." << endl;
			num_GFP_spot_per_worm = 1;
		}
		int num_GFP_worms = worms.countHighMagFluoWorms(GFP_img, intensity_threshold, area_threshold, num_GFP_spot_per_worm); // thresh_val and thresh_area values are guesses for now

		//For now, if we found at least 1 fluo worm in the image then set the fluo type -- will need to adjust later for case of multiple worms and multiple fluo
		tracked_worm.current_phenotype.GFP_type = num_GFP_worms == 0 ? NON_GFP : GFP;

		// If the expected phenotype is not empty, then quit once GFP is not as expected
		if (!phenotyping_method_being_used.getWantedPhenotype().is_empty()) {
			if (phenotyping_method_being_used.getWantedPhenotype().GFP_type != tracked_worm.current_phenotype.GFP_type) {
				cout << "Tracked worm is of the incorrect GFP phenotype: " << (GFP_types)tracked_worm.current_phenotype.GFP_type
					<< " Exit the phenotyping" << endl;
				return false;
			}
			else {
				cout << "Tracked worm is of the correct GFP phenotype: " << (GFP_types)tracked_worm.current_phenotype.GFP_type << endl;
			}
		}

		// If the expected phenotype is empty, then continue phenotyping
		else {
			cout << "Tracked worm has GFP phenotype: " << (GFP_types)tracked_worm.current_phenotype.GFP_type << endl;
		}
	}

	if (check_RFP) {
		// Load parameters for intensity and area threshold for phenotyping
		json RFP_phenotype_config = Config::getInstance().getConfigTree("phenotyping")["RFP"];
		int intensity_threshold = RFP_phenotype_config["intensity"].get<int>();
		int area_threshold = RFP_phenotype_config["area"].get<int>();

		Mat RFP_img;

		int old_worm_finding_mode = worms.worm_finding_mode;
		worms.worm_finding_mode = worms.WAIT;
		// Enable fluorescence imaging
		ImagingMode(RFP_IMAGING, Grbl2, camFact);

		// Get the fluorescence frame
		worms.img_high_mag.copyTo(RFP_img);

		// Turn back to brightfield imaging
		ImagingMode(BRIGHTFIELD, Grbl2, camFact);
		worms.worm_finding_mode = old_worm_finding_mode;

		if (num_RFP_spot_per_worm == 0) {
			cout << "No value given for the number of expected RFP spots per worm. Using default of 1." << endl;
			num_RFP_spot_per_worm = 1;
		}
		int num_RFP_worms = worms.countHighMagFluoWorms(RFP_img, intensity_threshold, area_threshold, num_RFP_spot_per_worm); // thresh_val and thresh_area values are guesses for now

		//For now, if we found at least 1 fluo worm in the image then set the fluo type -- will need to adjust later for case of multiple worms and multiple fluo
		tracked_worm.current_phenotype.RFP_type = num_RFP_worms == 0 ? NON_RFP : RFP;

		// If the expected phenotype is not empty, then quit once RFP is not as expected
		if (!phenotyping_method_being_used.getWantedPhenotype().is_empty()) {
			if (phenotyping_method_being_used.getWantedPhenotype().RFP_type != tracked_worm.current_phenotype.RFP_type) {
				cout << "Tracked worm is of the incorrect RFP phenotype: " << (RFP_types)tracked_worm.current_phenotype.RFP_type
					<< " Exit the phenotyping" << endl;
				return false;
			}
			else {
				cout << "Tracked worm is of the correct RFP phenotype: " << (RFP_types)tracked_worm.current_phenotype.RFP_type << endl;
			}
		}

		// If the expected phenotype is empty, then continue phenotyping
		else {
			cout << "Tracked worm has RFP phenotype: " << (RFP_types)tracked_worm.current_phenotype.RFP_type << endl;
		}
	}



	// Clear the return phenotype list first for safety
	phenotypes_inferred.clear();
	for (int i = 0; i < num_worms; i++) {
		phenotypes_inferred.push_back(tracked_worm.current_phenotype);
	}


	// Evaluate the resulting phenotyping vs the desired phenotyping
	// Note that because there is a necessary time delay between the images being used for gender/morphology and fluo determination
	// (Because we must change the lighting and wait for exposure to get fluo image) we can't necessarily correlate which worm has the 
	// gender/morphology results to which worm has the fluo results if there are multiple worms. 
	// So instead we just look to see if the desired phenotypes are present at all, and not worry about if they are all on the same worm.
	// Then if all the desired phenotypes are found we can do an intermediate pick.
	// In the future we may want to try to correlate the fluo spots to the nearest masks from the python server to try to correlate the results
	// from both images, and see if that works well.
	if (check_sex || check_morph || check_stage) {
		bool desired_sex_found = false;
		bool desired_morph_found = false;
		bool desired_stage_found = false;



		for (int i = 0; i < num_worms; i++) {
			// Segment the worm (this will set the physical characteristics of the worm - contours/centerline/sex/head-tail locations/morphology
			AnthonysTimer segment;
			tracked_worm.PhenotypeWormFromMaskRCNNResults(worm_img, results, i, 30, 0.45, false);
			cout << "Time to segment worm from mask rcnn results: " << segment.getElapsedTime() << endl;

			// Look at gender results if necessary
			if (check_sex) {

				// If the expected phenotype is not empty
				if (!phenotyping_method_being_used.getWantedPhenotype().is_empty()) {
					if (phenotyping_method_being_used.getWantedPhenotype().SEX_type != tracked_worm.current_phenotype.SEX_type) {
						cout << "Worm" << i + 1 << " is of the incorrect gender: " << (sex_types)tracked_worm.current_phenotype.SEX_type << endl;
						if (tracked_worm.current_phenotype.SEX_type == UNKNOWN_SEX) {
							worms.unknown_pheno_in_high_mag = true;
						}
						//return false;
					}
					else {
						cout << "Tracked worm is the correct gender: " << (sex_types)tracked_worm.current_phenotype.SEX_type << endl;
						desired_sex_found = true;
						//break;
					}
				}

				// If the expected phenotype is empty
				else {
					cout << "Tracked worm has the sex phenotype: " << (sex_types)tracked_worm.current_phenotype.SEX_type << endl;
					// TODO: Need to distinguish sex phenotype of different worms
				}
			}

			// Look at morphology results if necessary
			if (check_morph) {
				if (phenotyping_method_being_used.getWantedPhenotype().MORPH_type == DUMPY &&
					tracked_worm.dumpy_ratio > tracked_worm.dumpy_ratio_thresh * 0.92 && 
					tracked_worm.dumpy_ratio < tracked_worm.dumpy_ratio_thresh * 1.08) 
				{
					// Dumpy results are too terminate to be certain, so gather a couple extra images and re-test for dumpiness.
					tracked_worm.current_phenotype.MORPH_type = clarifyDumpyStatus(worms, tracked_worm.current_phenotype.MORPH_type == DUMPY) ? DUMPY : NORMAL;
				}

				// If the expected phenotype is not empty
				if (!phenotyping_method_being_used.getWantedPhenotype().is_empty()) {
					if (phenotyping_method_being_used.getWantedPhenotype().MORPH_type != tracked_worm.current_phenotype.MORPH_type) {
						cout << "Worm" << i + 1 << " is of the incorrect morphology: " << (morph_types)tracked_worm.current_phenotype.MORPH_type << endl;

						if (tracked_worm.current_phenotype.MORPH_type == UNKNOWN_MORPH) {
							worms.unknown_pheno_in_high_mag = true;
						}
					}
					else {
						cout << "Tracked worm is the correct morphology: " << (morph_types)tracked_worm.current_phenotype.MORPH_type << endl;
						desired_morph_found = true;

						// Save the mask for debugging
						/*Mat debug_mask;
						Mat debug_img;
						worm_img.copyTo(debug_img);
						results.worms_mask.copyTo(debug_mask);
						debug_mask *= 35;
						string datetime_string = getCurrentTimeString(false, true);
						string verif_img_fpath = "C:\\WormPicker\\recordings\\verification_images\\dumpy_classify\\" + datetime_string + "_dumpy_img.png";
						string verif_mask_fpath = "C:\\WormPicker\\recordings\\verification_images\\dumpy_classify\\" + datetime_string + "_dumpy_mask.png";
						vector<int> params{ CV_IMWRITE_PNG_COMPRESSION, 0 };
						imwrite(verif_img_fpath, debug_img, params);
						imwrite(verif_mask_fpath, debug_mask, params);*/
					}
				}

				// If the expected phenotype is empty
				else {
					cout << "Tracked worm has the morphology phenotype: " << (morph_types)tracked_worm.current_phenotype.MORPH_type << endl;
				}

			}

			// Look at stage results if necessary
			if (check_stage) {
				// If the expected phenotype is not empty
				if (!phenotyping_method_being_used.getWantedPhenotype().is_empty()) {
					if (phenotyping_method_being_used.getWantedPhenotype().STAGE_type != tracked_worm.current_phenotype.STAGE_type) {
						cout << "Worm" << i + 1 << " is of the incorrect stage: " << (stage_types)tracked_worm.current_phenotype.STAGE_type << endl;

						if (tracked_worm.current_phenotype.STAGE_type == UNKNOWN_STAGE) {
							worms.unknown_pheno_in_high_mag = true;
						}
						//return false;
					}
					else {
						cout << "Tracked worm is the correct stage: " << (stage_types)tracked_worm.current_phenotype.STAGE_type << endl;
						desired_stage_found = true;
						//break;
					}
				}

				// If the expected phenotype is empty
				else {
					cout << "Tracked worm has the stage phenotype: " << (stage_types)tracked_worm.current_phenotype.STAGE_type << endl;
				}

			}


			// append the Phenotype of each worm to the return list
			// TODO: 
			// Right now the fluorescence spots are not correlated with the worm contours segmented by mask-rcnn.
			// Right now the we assume all the worms in high-mag have fluo if any bright spots detected in the whole images;
			// and assume all the worms in high-mag are non-fluo if no bright spots detected in the whole image.
			// The next step would be correlated with fluo spots with worm contours in high-mag image.

			phenotypes_inferred[i] = tracked_worm.current_phenotype;
		}

		// If the expected phenotype is not empty
		if (!phenotyping_method_being_used.getWantedPhenotype().is_empty()) {
			if (phenotyping_method_being_used.getWantedPhenotype().SEX_type && !desired_sex_found) {
				cout << "Tracked worm is of the incorrect gender: " << (sex_types)tracked_worm.current_phenotype.SEX_type << endl;
				return false;
			}

			if (phenotyping_method_being_used.getWantedPhenotype().MORPH_type && !desired_morph_found) {
				cout << "Tracked worm is of the incorrect morphology: " << (morph_types)tracked_worm.current_phenotype.MORPH_type << endl;
				return false;
			}

			if (phenotyping_method_being_used.getWantedPhenotype().STAGE_type && !desired_stage_found) {
				cout << "Tracked worm is of the incorrect stage: " << (stage_types)tracked_worm.current_phenotype.STAGE_type << endl;
				return false;
			}
		}
	}


	return true;
}


// Working on correlating fluo spots with segmentation results from mask-rcnn
bool executePhenotypeV2(WormPick &Pk, const PlateTray& Tr, OxCnc& Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder& worms, CameraFactory& camFact, int& num_worms, vector<Phenotype>& phenotypes_inferred, bool save_frame) {
	worms.unknown_pheno_in_high_mag = false;

	// args[0] whether check sex
	// 0: No sex phenotyping
	// 1: Want sex phenotyping
	bool check_sex = stoi(Tr.getCurrentScriptStepInfo().args[0]) != 0;

	// args[1] whether check GFP
	// 0: No GFP phenotyping
	// 1: Want GFP phenotyping
	bool check_GFP = stoi(Tr.getCurrentScriptStepInfo().args[1]) != 0;

	// args[8] the number of GFP spots expected on a single worm (obviously only needed if you're searching for fluo worms)
	int num_GFP_spot_per_worm = stoi(Tr.getCurrentScriptStepInfo().args[8]);

	// args[2] whether check RFP
	// 0: No RFP phenotyping
	// 1: Want RFP phenotyping
	bool check_RFP = stoi(Tr.getCurrentScriptStepInfo().args[2]) != 0;

	// args[9] the number of RFP spots expected on a single worm (obviously only needed if you're searching for fluo worms)
	int num_RFP_spot_per_worm = stoi(Tr.getCurrentScriptStepInfo().args[9]);

	// args[3] whether check morphology
	// 0: No morphology phenotyping
	// 1: Want morphology phenotyping
	bool check_morph = stoi(Tr.getCurrentScriptStepInfo().args[3]) != 0;

	// args[4] whether check stage
	// 0: No stage phenotyping
	// 1: Want stage phenotyping
	bool check_stage = stoi(Tr.getCurrentScriptStepInfo().args[4]) != 0;

	// Grab the worm image
	// To avoid false positive of low-mag tracking,
	// image sent to mask_rcnn to determine whether have any worm in the high-mag before executing any phenotyping
	Mat any_worm_image;
	worms.img_high_mag_color.copyTo(any_worm_image);

	Worm& tracked_worm = *(worms.getTrackedWorm());

	// Feed the image to mask-rcnn server and get results to see whether have worm in the high mag.
	cout << "Sending image to Python server" << endl;
	maskRCNN_results any_worm_results = maskRcnnRemoteInference(any_worm_image);
	cout << "Results received. " << endl;

	// Get the number of worms in the returned mask
	if (any_worm_results.num_worms > 1) {
		// If we have multiple worms in the image save it for inspection to double check
		/*Mat debug_mask;
		Mat debug_img;
		worms.img_high_mag_color.copyTo(debug_img);
		results.worms_mask.copyTo(debug_mask);
		debug_mask *= 35;*/
		/*string datetime_string = getCurrentTimeString(false, true);
		string verif_img_fpath = "C:\\WormPicker\\recordings\\verification_images\\high_mag_worm_count\\" + datetime_string + "_" + to_string(num_worms) + "worms_img.png";
		string verif_mask_fpath = "C:\\WormPicker\\recordings\\verification_images\\high_mag_worm_count\\" + datetime_string + "_" + to_string(num_worms) + "worms_mask.png";
		vector<int> params{ CV_IMWRITE_PNG_COMPRESSION, 0 };
		imwrite(verif_img_fpath, debug_img, params);
		imwrite(verif_mask_fpath, debug_mask, params);*/
	}

	if (any_worm_results.num_worms == 0) {
		cout << "Mask RCNN network did not find any worms in the high mag image! Exiting phenotyping." << endl;
		return false;
	}


	// Exit if No phenotyping desired.
	if (!check_sex && !check_GFP && !check_RFP && !check_morph && !check_stage)
		return true;


	// Clear the return phenotype list first for safety
	phenotypes_inferred.clear();

	// Status of phenotyping
	bool desired_sex_found = false;
	bool desired_GFP_found = false;
	bool desired_RFP_found = false;
	bool desired_morph_found = false;
	bool desired_stage_found = false;


	// Variables for fluorescence frames
	Mat gfp_frame;
	Mat rfp_frame;
	Mat* ptr_gfp_frame = nullptr;
	Mat* ptr_rfp_frame = nullptr;

	// Center the worm again
    centerTrackedWormInHighMag(Ox, worms, Pk, Tr);

    // Grab the worm image right after fluorescence phenotyping
    Mat worm_img;
    worms.img_high_mag.copyTo(worm_img);

    // Feed the image to mask-rcnn server and get results.
    cout << "Sending image to Python server" << endl;
    maskRCNN_results results = maskRcnnRemoteInference(worm_img);
    cout << "Results received. " << endl;

	// If GFP or RFP phenotyping required then grab frames for later processing
	if (check_GFP || check_RFP) {

		int old_worm_finding_mode = worms.worm_finding_mode;
		worms.worm_finding_mode = worms.WAIT;

		if (check_GFP) {

			// Enable fluorescence imaging
			ImagingMode(GFP_IMAGING, Grbl2, camFact);

			// Get the fluorescence frame
			worms.img_high_mag.copyTo(gfp_frame);

			// Assign pointer
			ptr_gfp_frame = &gfp_frame;
		}

		if (check_RFP) {
			// Enable fluorescence imaging
			ImagingMode(RFP_IMAGING, Grbl2, camFact);

			// Get the fluorescence frame
			worms.img_high_mag.copyTo(rfp_frame);

			// Assign pointer
			ptr_rfp_frame = &rfp_frame;
		}

		// Turn back to brightfield imaging
		ImagingMode(BRIGHTFIELD, Grbl2, camFact);
		worms.worm_finding_mode = old_worm_finding_mode;
	}

	// If save frame flag is true then write images to hard disk
	if (save_frame) {
		string datetime_string = getCurrentTimeString(false, true);
		string BF_img_name = BF_img_dir + datetime_string + "_brightfield.png";
		string GFP_img_name = GFP_img_dir + datetime_string + "_GFP.png";
		string RFP_img_name = RFP_img_dir + datetime_string + "_RFP.png";

		vector<int> params{ CV_IMWRITE_PNG_COMPRESSION, 0 };
		imwrite(BF_img_name, worm_img, params);
		if (ptr_gfp_frame != nullptr) imwrite(GFP_img_name, *ptr_gfp_frame, params);
		if (ptr_rfp_frame != nullptr) imwrite(RFP_img_name, *ptr_rfp_frame, params);
	}

	// Number of worm found
	num_worms = results.num_worms;

	// Iterate through each worm identified in the high-mag image
	for (int i = 0; i < num_worms; i++) {

		// Segment the worm (this will set the physical characteristics of the worm - contours/centerline/sex/head-tail locations/morphology
		AnthonysTimer segment;
		tracked_worm.PhenotypeWormFromMaskRCNNResults(worm_img, results, i, 30, 0.45, false, ptr_gfp_frame, ptr_rfp_frame);
		cout << "Time to segment worm from mask rcnn results: " << segment.getElapsedTime() << endl;

		// Look at gender results if necessary
		if (check_sex) {

			// If the expected phenotype is not empty
			if (!phenotyping_method_being_used.getWantedPhenotype().is_empty()) {
				if (phenotyping_method_being_used.getWantedPhenotype().SEX_type != tracked_worm.current_phenotype.SEX_type) {
					cout << "Worm" << i + 1 << " is of the incorrect gender: " << (sex_types)tracked_worm.current_phenotype.SEX_type << endl;
					if (tracked_worm.current_phenotype.SEX_type == UNKNOWN_SEX) {
						worms.unknown_pheno_in_high_mag = true;
					}
					//return false;

					// break the loop and go to next worm
					continue;
				}
				else {
					cout << "Tracked worm is the correct gender: " << (sex_types)tracked_worm.current_phenotype.SEX_type << endl;
					desired_sex_found = true;
					//break;
				}
			}

			// If the expected phenotype is empty
			else {
				cout << "Tracked worm has the sex phenotype: " << (sex_types)tracked_worm.current_phenotype.SEX_type << endl;
				// TODO: Need to distinguish sex phenotype of different worms
			}
		}

		// Look at GFP results if necessary
		if (check_GFP) {
			// If the expected phenotype is not empty
			if (!phenotyping_method_being_used.getWantedPhenotype().is_empty()) {
				if (phenotyping_method_being_used.getWantedPhenotype().GFP_type != tracked_worm.current_phenotype.GFP_type) {
					cout << "Tracked worm is of the incorrect GFP phenotype: " << (GFP_types)tracked_worm.current_phenotype.GFP_type << endl;
					if (tracked_worm.current_phenotype.GFP_type == UNKNOWN_GFP) {
						worms.unknown_pheno_in_high_mag = true;
					}
					//return false;

					// break the loop and go to next worm
					continue;
				}
				else {
					cout << "Tracked worm is of the correct GFP phenotype: " << (GFP_types)tracked_worm.current_phenotype.GFP_type << endl;
					desired_GFP_found = true;
				}
			}

			// If the expected phenotype is empty
			else {
				cout << "Tracked worm has GFP phenotype: " << (GFP_types)tracked_worm.current_phenotype.GFP_type << endl;
			}
		}



		// Look at RFP results if necessary
		if (check_RFP) {
			// If the expected phenotype is not empty
			if (!phenotyping_method_being_used.getWantedPhenotype().is_empty()) {
				if (phenotyping_method_being_used.getWantedPhenotype().RFP_type != tracked_worm.current_phenotype.RFP_type) {
					cout << "Tracked worm is of the incorrect RFP phenotype: " << (RFP_types)tracked_worm.current_phenotype.RFP_type << endl;
					if (tracked_worm.current_phenotype.RFP_type == UNKNOWN_RFP) {
						worms.unknown_pheno_in_high_mag = true;
					}
					//return false;

					// break the loop and go to next worm
					continue;
				}
				else {
					cout << "Tracked worm is of the correct RFP phenotype: " << (RFP_types)tracked_worm.current_phenotype.RFP_type << endl;
					desired_RFP_found = true;
				}
			}

			// If the expected phenotype is empty
			else {
				cout << "Tracked worm has RFP phenotype: " << (RFP_types)tracked_worm.current_phenotype.RFP_type << endl;
			}
		}

		// Look at morphology results if necessary
		if (check_morph) {
			if (phenotyping_method_being_used.getWantedPhenotype().MORPH_type == DUMPY &&
				tracked_worm.dumpy_ratio > tracked_worm.dumpy_ratio_thresh * 0.92 &&
				tracked_worm.dumpy_ratio < tracked_worm.dumpy_ratio_thresh * 1.08)
			{
				// Dumpy results are too terminate to be certain, so gather a couple extra images and re-test for dumpiness.
				tracked_worm.current_phenotype.MORPH_type = clarifyDumpyStatus(worms, tracked_worm.current_phenotype.MORPH_type == DUMPY) ? DUMPY : NORMAL;
			}

			// If the expected phenotype is not empty
			if (!phenotyping_method_being_used.getWantedPhenotype().is_empty()) {
				if (phenotyping_method_being_used.getWantedPhenotype().MORPH_type != tracked_worm.current_phenotype.MORPH_type) {
					cout << "Worm" << i + 1 << " is of the incorrect morphology: " << (morph_types)tracked_worm.current_phenotype.MORPH_type << endl;

					if (tracked_worm.current_phenotype.MORPH_type == UNKNOWN_MORPH) {
						worms.unknown_pheno_in_high_mag = true;
					}

					// break the loop and go to next worm
					continue;
				}
				else {
					cout << "Tracked worm is the correct morphology: " << (morph_types)tracked_worm.current_phenotype.MORPH_type << endl;
					desired_morph_found = true;

					// Save the mask for debugging
					/*Mat debug_mask;
					Mat debug_img;
					worm_img.copyTo(debug_img);
					results.worms_mask.copyTo(debug_mask);
					debug_mask *= 35;
					string datetime_string = getCurrentTimeString(false, true);
					string verif_img_fpath = "C:\\WormPicker\\recordings\\verification_images\\dumpy_classify\\" + datetime_string + "_dumpy_img.png";
					string verif_mask_fpath = "C:\\WormPicker\\recordings\\verification_images\\dumpy_classify\\" + datetime_string + "_dumpy_mask.png";
					vector<int> params{ CV_IMWRITE_PNG_COMPRESSION, 0 };
					imwrite(verif_img_fpath, debug_img, params);
					imwrite(verif_mask_fpath, debug_mask, params);*/
				}
			}

			// If the expected phenotype is empty
			else {
				cout << "Tracked worm has the morphology phenotype: " << (morph_types)tracked_worm.current_phenotype.MORPH_type << endl;
			}
		}

		// Look at stage results if necessary
		if (check_stage) {
			// If the expected phenotype is not empty
			if (!phenotyping_method_being_used.getWantedPhenotype().is_empty()) {
				if (phenotyping_method_being_used.getWantedPhenotype().STAGE_type != tracked_worm.current_phenotype.STAGE_type) {
					cout << "Worm" << i + 1 << " is of the incorrect stage: " << (stage_types)tracked_worm.current_phenotype.STAGE_type << endl;

					if (tracked_worm.current_phenotype.STAGE_type == UNKNOWN_STAGE) {
						worms.unknown_pheno_in_high_mag = true;
					}
					//return false;

					// break the loop and go to next worm
					continue;
				}
				else {
					cout << "Tracked worm is the correct stage: " << (stage_types)tracked_worm.current_phenotype.STAGE_type << endl;
					desired_stage_found = true;
					//break;
				}
			}

			// If the expected phenotype is empty
			else {
				cout << "Tracked worm has the stage phenotype: " << (stage_types)tracked_worm.current_phenotype.STAGE_type << endl;
			}

		}


		// Append the phenotype to the return list
		phenotypes_inferred.push_back(tracked_worm.current_phenotype);

	}


	// If the expected phenotype is not empty
	if (!phenotyping_method_being_used.getWantedPhenotype().is_empty()) {
		if (phenotyping_method_being_used.getWantedPhenotype().SEX_type && !desired_sex_found) {
			cout << "Tracked worm is of the incorrect gender: " << (sex_types)tracked_worm.current_phenotype.SEX_type << endl;
			return false;
		}

		if (phenotyping_method_being_used.getWantedPhenotype().GFP_type && !desired_GFP_found) {
			cout << "Tracked worm is of the incorrect GFP: " << (GFP_types)tracked_worm.current_phenotype.GFP_type << endl;
			return false;
		}

		if (phenotyping_method_being_used.getWantedPhenotype().RFP_type && !desired_RFP_found) {
			cout << "Tracked worm is of the incorrect RFP: " << (RFP_types)tracked_worm.current_phenotype.RFP_type << endl;
			return false;
		}


		if (phenotyping_method_being_used.getWantedPhenotype().MORPH_type && !desired_morph_found) {
			cout << "Tracked worm is of the incorrect morphology: " << (morph_types)tracked_worm.current_phenotype.MORPH_type << endl;
			return false;
		}

		if (phenotyping_method_being_used.getWantedPhenotype().STAGE_type && !desired_stage_found) {
			cout << "Tracked worm is of the incorrect stage: " << (stage_types)tracked_worm.current_phenotype.STAGE_type << endl;
			return false;
		}
	}

	return true;

}


/// Deppricated version of HeatPick, the new WormPicker does not use pulse modulation to heat the pick
//bool executeHeatPick(WormPick &Pk, const PlateTray& Tr, OxCnc& Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock) {
//	bool success = false;
//	
//	//// Require that a lid NOT be lifted for plate safety
//	//if (Ox.getLidIsRaised()) {
//	//	cout << "Could not heat because a lid is in the air (not safe)" << endl;
//	//	return false;
//	//}
//
//	// Move gantry UP (Comment for Saving time, Maybe uncomment later)
//	/*if (!Ox.raiseGantry()) {
//		std::cout << "Could not heat because failed to move gantry up" << endl;
//		return false;
//	}*/
//
//
//
//	// Move pick to focus pos
//		//Pk.movePickToStartPos(Grbl2,true,true);
//
//	// Require that the pick tip is visible
//	/*	boost_sleep_ms(250);
//		if (Pk.getPickTip().x <= 0) {
//			std::cout << "Could not heat because can't see pick tip" << endl;
//			return false;
//		}*/
//
//	// Close the switch-relays first to enable heating (disables touch sensation)
//	LabJack_SetFioN(Grbl2.getLabJackHandle(), 6, 0);
//
//
//	// Heat for the length (in s) from arg0
//	string str("");
//	double heating_time = Tr.getCurrentScriptStepInfo().args[0];
//	double freq = Tr.getCurrentScriptStepInfo().args[1];
//	double duty_cycle = Tr.getCurrentScriptStepInfo().args[2];
//
//	if (duty_cycle > 1 || heating_time > 15) {
//		cout << "Duty cycle or heating time too high!! Fail to heat!" << endl;
//	}
//	else {
//		LabJack_PWM_DAC(Grbl2.getLabJackHandle(), 1, heating_time, freq, duty_cycle, str);
//		success = true;
//	}
//
//	// Open the switch-relays first to disable heating (re-enables touch sensation)
//	LabJack_SetFioN(Grbl2.getLabJackHandle(), 6, 5);
//
//	// Wait a few seconds to cool
//	// boost_sleep_ms(1000);
//
//	// Reset the capacitive sensor, takes some time to recover after opening both heating relays.
//	Pk.powerCycleCapSensor(Grbl2.getLabJackHandle());
//
//	// Move pick away
//	//Pk.movePickSlightlyAway();
//	//Pk.movePickToStartPos(Grbl2, true, true, 0, false);
//
//	// Return gantry to height
//	/*if (!Ox.lowerGantry())
//		return false;*/
//
//	return success;
//}



// Perform sterilization to pick by sending steady electric current (NOT pulse modulation like old version)
bool executeHeatPick(WormPick &Pk, PlateTray &Tr, OxCnc &Grbl2) {

	// Heating time (in second)
	int secs_to_heat = stoi(Tr.getCurrentScriptStepInfo().args[0]);

	// Cooling time (in second)
	double secs_to_cool = stod(Tr.getCurrentScriptStepInfo().args[1]);

	// Dac number setup for heating pick at LabJack
	int DacNum = 0;

	Pk.heatPick(Grbl2, DacNum, secs_to_heat, secs_to_cool);

	return true;
}


// Need to check whether this this script is deprecated
bool executePhenotypeFluo(WormPick &Pk, const PlateTray& Tr, OxCnc& Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder &worms, CameraFactory& camFact) {
	
	// args[0], args[1]: the starting and ending coordinates of body portion expected to carry fluorescence, i.e. portion of interest (POI)
	double POI_start = stod(Tr.getCurrentScriptStepInfo().args[0]);
	double POI_end = stod(Tr.getCurrentScriptStepInfo().args[1]);

	// args[2]: thresholding value (in terms of standard deviation of control intensity) that determine whether has fluorescence signal in POI
	// E.g. args[2] = 2 means POI is considered to have fluorescence if (intensity in POI - control intensity) > 2*SD.
	double thresh = stod(Tr.getCurrentScriptStepInfo().args[2]);

	// args[3] is the time latency (100*ms) of switching brightfield and fluorescence imaging
	double delay = stod(Tr.getCurrentScriptStepInfo().args[3]);

	// args[4] is the bin number of segmentation along the body coordinate
	int bin_num = stoi(Tr.getCurrentScriptStepInfo().args[4]);

	// args[5] is exposure time (ms) set to fluorescence camera to image fluorescent worm
	int expo_time = stoi(Tr.getCurrentScriptStepInfo().args[5]);


	Mat img, fluo_img;
	// Get the brightfield frame right before the fluorescence imaging
	worms.img_high_mag.copyTo(img);

	// Enable fluorescence imaging
	ImagingMode(GFP_IMAGING, Grbl2, camFact);

	// Get the fluorescence frame
	worms.img_high_mag.copyTo(fluo_img);

	// Turn back to brightfield imaging
	ImagingMode(BRIGHTFIELD, Grbl2, camFact);

	// Phenotype fluorescence -- go back and make this link up with worms instead of creating new WS
	string fname_intensity_profile = "C:\\Dropbox\\WormPicker (shared data)\\WormIntensityTemplate.csv";
	Worm current_worm;
	current_worm.segmentWorm(img);
	current_worm.fluoProfile(fluo_img);

	return true;
}

// Arg0 == 0: Go to with raise (default) , 1: Go directly to plate
// Arg1 == 1: Sterilize during motion
// Arg2 == 1: Add random offset
// Arg3 == 1: Move the pick out of view of the camera (we typically only care about this if we're moving to the source plate)
// Arg4 == 1: Manage lids when moving to the plate
// Arg5 == 1: Perform laser autofocus after moving (Typically used here if moving to destination plate to drop a worm)
bool executeMoveToPlate(WormPick& Pk, PlateTray& Tr, OxCnc& Ox, OxCnc& Grbl2, 
						StrictLock<OxCnc>& oxLock, StrictLock<OxCnc>& grbl2Lock, WormFinder& worms, CameraFactory& camFact) {

	if (stoi(Tr.getCurrentScriptStepInfo().args[4]) == 1) {
		PlateTray dummy_tray(Tr);
		//First check if we need to handle any lids for this plate. 
		Pk.lid_handler.manageLids(dummy_tray.getCurrentScriptStepInfo().id, dummy_tray, Ox);
	}

	AnthonysTimer testNoBlockHeating;

	//// Open the switch-relays first to disable heating (re-enables touch sensation)
	//LabJack_SetFioN(Grbl2.getLabJackHandle(), 6, 0);

	// Arg1 == 1: Sterilize during motion
	// If requested, Turn the pick sterilization on so that we heat the pick while moving.
	// NOTE: If you want to sterilize while moving you MUST re-open the switch-relays yourself before powercycling the
	// capacitive sensor and before performing any picking actions.
	if (stoi(Tr.getCurrentScriptStepInfo().args[1]) == 1) {
		//// Close the switch-relays first to enable heating (disables touch sensation)
		//LabJack_SetFioN(Grbl2.getLabJackHandle(), 6, 5);

		//string err_string = string("");

		//// Send request to labjack to heat pic for 2 seconds
		//LabJack_Pulse_DAC(Grbl2.getLabJackHandle(), 1, 2, 0, err_string, 5, 0);
		Pk.heatPickInThread(Grbl2, Pk, 0, 2, 0.2);
	}
	cout << "Timer after heating request sent: " << testNoBlockHeating.getElapsedTime() << endl;

	// Moving with a lid is not allowed. Put down any lids first
	//executeLowerLid(Pk, Ox, Grbl2, oxLock, grbl2Lock);

	Point3d target_pos = Tr.getCurrentPosition();



	// If requested, add a random offset to plate center
	// Arg2 == 1: Add random offset
	if (stoi(Tr.getCurrentScriptStepInfo().args[2]) == 1) {
		double x_offset = 0;
		double y_offset = 0;
		executeRandomDropOffset(Ox, x_offset, y_offset);

		target_pos.x += x_offset;
		target_pos.y += y_offset;

	}

	oxLock.lock();
	//Pk.movePickAway();
	//Pk.movePickSlightlyAway();
	Pk.movePickSlightlyAbovePlateHeight(stoi(Tr.getCurrentScriptStepInfo().args[3]) != 0); // 3rd script argument is true if we are moving to the source plate.
	
	bool success = false;

	// Get the current position

	Point3d old_pos_grbl2 = Grbl2.getXYZ();
	// Arg0 = 0: Go to with raise (default)
	if (Ox.isConnected() && Grbl2.isConnected() && stoi(Tr.getCurrentScriptStepInfo().args[0]) == 0) {
		success = Ox.goToAbsoluteWithRaise(target_pos, oxLock, true, 20);
	}
	
	// Arg0 = 1: Go to directly
	else if (Ox.isConnected() && Grbl2.isConnected() && stoi(Tr.getCurrentScriptStepInfo().args[0]) == 1) {
		success = Ox.goToAbsolute(target_pos, true, 25);

		success = success && Grbl2.goToAbsolute(target_pos.x, old_pos_grbl2.y, target_pos.x, true, 25);
	}

	// Bad arguments
	else {
		std::cout << "Bad arguments to MoveToPlate" << endl;
		
	}

	oxLock.unlock();
	//boost_sleep_ms(100);
	cout << "***** Move to plate complete *****" << endl;
	worms.updatePickableRegion(Ox.getXYZ(), Tr.getCurrentPosition(), Pk.dydOx);

	cout << "New pickable region: " << worms.pickable_region.pickable_center << endl;

	// Perform auto focus if desired (Typically for if we moved to a destination plate to drop a worm)
	if (stoi(Tr.getCurrentScriptStepInfo().args[5]) == 1) {
		executeAutoFocus(Ox, worms, camFact, Tr);
	}

	worms.image_timer.setToZero(); // We moved the stage so we must reset the image grabber timer for Wormfinder

	/// Save the frames for Siming recognizing the well center
	//Mat well_img;
	//worms.imggray.copyTo(well_img);
	//string img_fpath = "C:\\Users\\Yuchi\\Dropbox\\Siming_John_Shared_folder\\Well_picture\\row_" +
	//	to_string(Tr.getCurrentScriptStepInfo().id.row)+ 
	//	"_col_" + to_string(Tr.getCurrentScriptStepInfo().id.col)+
	//	"_well_img.png";
	//vector<int> params{ CV_IMWRITE_PNG_COMPRESSION, 0 };
	//imwrite(img_fpath, well_img, params);

	return success;
}

// Arg0 == 0: Go to with raise (default) , 1: Go directly to plate
// Arg1 == 1: Sterilize during motion
// Arg2 == 1: Add random offset
// Arg3 == 1: Move the pick out of view of the camera (we typically only care about this if we're moving to the source plate)
// Arg4 == 1: Manage lids when moving to the plate
// Arg5 == 1: Perform laser autofocus after moving (Typically used here if moving to destination plate to drop a worm)
// Arg6 == 1: Read the barcode and look up plate information in database
// Arg7 == 1: Perform a dummy database write (for testing)
bool executeMoveToPlate(WormPick& Pk, PlateTray& Tr, OxCnc& Ox, OxCnc& Grbl2,
	StrictLock<OxCnc>& oxLock, StrictLock<OxCnc>& grbl2Lock, WormFinder& worms, CameraFactory& camFact,
	PlateTracker& plateTracker) {

	if (stoi(Tr.getCurrentScriptStepInfo().args[4]) == 1) {
		PlateTray dummy_tray(Tr);
		//First check if we need to handle any lids for this plate. 
		Pk.lid_handler.manageLids(dummy_tray.getCurrentScriptStepInfo().id, dummy_tray, Ox);
	}

	AnthonysTimer testNoBlockHeating;

	// Arg1 == 1: Sterilize during motion
	// If requested, Turn the pick sterilization on so that we heat the pick while moving.
	// NOTE: If you want to sterilize while moving you MUST re-open the switch-relays yourself before powercycling the
	// capacitive sensor and before performing any picking actions.
	if (stoi(Tr.getCurrentScriptStepInfo().args[1]) == 1) {
		Pk.heatPickInThread(Grbl2, Pk, 0, 2, 0.2);
	}
	cout << "Timer after heating request sent: " << testNoBlockHeating.getElapsedTime() << endl;

	Point3d target_pos = Tr.getCurrentPosition();

	// If requested, add a random offset to plate center
	// Arg2 == 1: Add random offset
	if (stoi(Tr.getCurrentScriptStepInfo().args[2]) == 1) {
		double x_offset = 0;
		double y_offset = 0;
		executeRandomDropOffset(Ox, x_offset, y_offset);

		target_pos.x += x_offset;
		target_pos.y += y_offset;

	}

	oxLock.lock();
	Pk.movePickSlightlyAbovePlateHeight(stoi(Tr.getCurrentScriptStepInfo().args[3]) != 0); // 3rd script argument is true if we are moving to the source plate.

	bool success = false;

	// Get the current position

	Point3d old_pos_grbl2 = Grbl2.getXYZ();
	// Arg0 = 0: Go to with raise (default)
	if (Ox.isConnected() && Grbl2.isConnected() && stoi(Tr.getCurrentScriptStepInfo().args[0]) == 0) {
		success = Ox.goToAbsoluteWithRaise(target_pos, oxLock, true, 20);
	}

	// Arg0 = 1: Go to directly
	else if (Ox.isConnected() && Grbl2.isConnected() && stoi(Tr.getCurrentScriptStepInfo().args[0]) == 1) {
		success = Ox.goToAbsolute(target_pos, true, 25);

		success = success && Grbl2.goToAbsolute(target_pos.x, old_pos_grbl2.y, target_pos.x, true, 25);
	}

	// Bad arguments
	else {
		std::cout << "Bad arguments to MoveToPlate" << endl;

	}

	oxLock.unlock();
	cout << "***** Move to plate complete *****" << endl;
	worms.updatePickableRegion(Ox.getXYZ(), Tr.getCurrentPosition(), Pk.dydOx);

	cout << "New pickable region: " << worms.pickable_region.pickable_center << endl;

	// Perform auto focus if desired (Typically for if we moved to a destination plate to drop a worm)
	if (stoi(Tr.getCurrentScriptStepInfo().args[5]) == 1) {
		executeAutoFocus(Ox, worms, camFact, Tr);
	}

	worms.image_timer.setToZero(); // We moved the stage so we must reset the image grabber timer for Wormfinder

	// Arg6 == 1: Read the barcode and look up plate information in database
	if (stoi(Tr.getCurrentScriptStepInfo().args[6]) == 1) {
		cout << "Reading barcode..." << endl;
		executeTestReadBarcode(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact, plateTracker);
		cout << "Id decoded: " << plateTracker.getID() << endl;
        // TODO: comment back
//		cout << "Looking up id in database..." << endl;
//		Plate p = plateTracker.readCurrentPlate();
//		cout << "Plate id: " << p.id << endl;
//		cout << "Plate strain: " << p.strain << endl;
//		cout << "Plate phenotypes: " << phenotypesToString(p) << endl;
//		cout << "Plate generation number: " << p.genNum << endl;
//		cout << "Plate creation date: " << p.creationDate << endl;
	}

	// Arg7 == 1: Perform dummy database writes (for testing)
	if (stoi(Tr.getCurrentScriptStepInfo().args[7]) == 1) {
		Plate p = plateTracker.readCurrentPlate();
		plateTracker.writeCurrentPlate("testStrain", { {{HERM, GFP, NON_RFP, NORMAL, ADULT}, 2},
			{ {HERM, NON_GFP, NON_RFP, NORMAL, ADULT}, 4} }, 1, "2022-04-12");
		plateTracker.writeCurrentPlate(p.strain, p.phenotypes, p.genNum, p.creationDate);
	}

	return success;
}

bool executeFocusImage(WormPick& Pk, const PlateTray& Tr, OxCnc& Ox, StrictLock<OxCnc>& oxLock, WormFinder& worms) {

	// Ensure pick is clear

	/// Arg0 = 0: Move pick completely away
	/// Arg0 = 1: Move pick slightly away
	bool move_pick_slightly_away = true;
	if (stoi(Tr.getCurrentScriptStepInfo().args[0]) == 0) {
		move_pick_slightly_away = false;
	}

	/// Arg1 = 0: focus on high mag image
	/// Arg1 = 1: focus on low mag image
	bool is_low_mag = stoi(Tr.getCurrentScriptStepInfo().args[1]) != 0;
	
	// Run focuser
	//cout << "Focuser running at Low-mag image..." << endl;
	//focus_image(Ox, Pk, worms, 1); // Focus low-mag image first
	cout << "Focuser running at High-mag image..." << endl;
	focusImage(Pk, Ox, worms, is_low_mag, move_pick_slightly_away); // Focus high-mag image secondly

	// Store plate's focus height in PlateTray
	// TODO
	return true;
}


bool executeMoveRel(const PlateTray& Tr, OxCnc& Ox, StrictLock<OxCnc> &oxLock) {

	return Ox.goToRelative(stod(Tr.getCurrentScriptStepInfo().args[0]),
							stod(Tr.getCurrentScriptStepInfo().args[1]),
							stod(Tr.getCurrentScriptStepInfo().args[2]),
							1.0, true);

}

// DEPRECATED: Lid handling is now managed by LidHandlers class, and is automatically dealt with when called moveToPlate with lid handling script arg.
//bool executeMoveAndLift(WormPick& Pk, const PlateTray& Tr, OxCnc& Ox, OxCnc& Grbl2, StrictLock<OxCnc>& oxLock, StrictLock<OxCnc>& grbl2Lock) {
//	bool lifting = Tr.getCurrentScriptStepInfo().args[0];
//
//	cout << "Dummy: MoveAndLift: col: " << Tr.getCurrentId().col << ", row: " << Tr.getCurrentId().row << endl;
//	//return true;
//	
//	// Do nothing if already at the specified plate and lifted
//	Point3d PlatePos = Tr.getCurrentPosition();
//	if (PlatePos.x == Ox.getX() && PlatePos.y == Ox.getY() && Ox.getLidIsRaised()) {
//		std::cout << "Could not run MoveAndLift because already doing it!" << endl;
//		return true;
//	}
//
//	
//	// Lock CNC
//	oxLock.lock();
//
//	// Moving with a lid is not allowed. Put down any lids first
//	//executeLowerLid(Pk, Ox, Grbl2, oxLock, grbl2Lock, false);
//
//	// Clear the pick
//	Pk.movePickSlightlyAway();
//	bool success = false;
//
//	// Move directly to the pickup location for the lid (or just above it), skipping over the plate 
//
//	/// Move to just above the plate, Lid position, NOT COUNTING ANY SHIFT
//	Point3d LidPosRel = Ox.getPosLidShift();
//	double z_lid_height = Ox.getZPlateHeight();
//	//Point3d LidPosAbs = Point3d(PlatePos.x + LidPosRel.x, PlatePos.y + LidPosRel.y, z_lid_height + 10); //0
//
//
//	Point3d GrabberPosAbs = Point3d(PlatePos.x + LidPosRel.x, PlatePos.y + LidPosRel.y, LidPosRel.z); // the Z never changes for lid grabbing, so lidPosRel.z is actually an absolute value.
//
//	cout << "LidPosRel: " << LidPosRel << endl;
//	cout << "Grabber pos abs: " << GrabberPosAbs << endl;
//
//	//Point3d old_pos_grbl2 = Grbl2.getXYZ();
//	success = Ox.goToAbsolute(GrabberPosAbs, true);
//
//	if (!success)
//	{
//		cout << "goToAbsolute failed!" << endl;
//		return success;
//	} /// Exit if failed
//
//
//	// lift of drop the lid.
//	Ox.liftOrLowerLid(lifting);
//	oxLock.unlock();
//	return success;
//}

// DEPRECATED: Lid handling is now managed by LidHandlers class, and is automatically dealt with when called moveToPlate with lid handling script arg.
//// This method will go to a plate, lift its lid, then move to a drop off location and release the lid. It will do this in sequence for every plate specified
//// in the input parameter lift_and_drop_locs.
//// Note that this method can not be called directly as a script step because you must first build a vector of lift and drop locations, then pass them here.
//// So it must be called indirectly from other script steps, after the lift and drop locations have been constructed there.
//bool executeLiftAndDropLids(WormPick& Pk, const PlateTray& Tr, OxCnc& Ox, OxCnc& Grbl2, StrictLock<OxCnc>& oxLock, StrictLock<OxCnc>& grbl2Lock, vector<tuple<MatIndex, MatIndex>> lift_and_drop_locs) {
//	PlateTray dummy_tray(Tr);
//	for (auto lift_and_drop : lift_and_drop_locs) {
//		MatIndex lift_loc = get<0>(lift_and_drop);
//		MatIndex drop_loc = get<1>(lift_and_drop);
//
//		// Go to the pickup plate and lift its lid
//		dummy_tray.changeScriptArgs({ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 });
//		dummy_tray.changeScriptId(lift_loc);
//		dummy_tray.updateStepInfo();
//
//		executeMoveAndLift(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock);
//
//		// Go to the drop off plate and release the lid
//		dummy_tray.changeScriptArgs({ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 });
//		dummy_tray.changeScriptId(drop_loc);
//		dummy_tray.updateStepInfo();
//
//		executeMoveAndLift(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock);	
//	}
//
//	return true;
//
//}

bool executePlateMapping(WormPick& Pk, const PlateTray& Tr, OxCnc& Ox, OxCnc& Grbl2, StrictLock<OxCnc>& oxLock, StrictLock<OxCnc>& grbl2Lock, WormFinder& worms, CameraFactory& camFact) {
	
	// Get the script action;
	/* Scan mode:
		MAPPING = 0 = scan every FOV of the whole plate
		FIND_WORM = 1 = scan the sequence of FOVs, stop once a POPULATED region is found
	*/
	PlateMap::scan_mode sc_mode = (PlateMap::scan_mode) stoi(Tr.getCurrentScriptStepInfo().args[0]);
	
	// STEP 1: Get the plate parameters from config file
	Point3d PlatePos = Tr.getCurrentPosition(); // TO DO: Get from config 
	float Diameter = 60; // TO DO: Get from config
	float FOV = 12; // TO DO: Get from config
	Size FOV_size = worms.imggray.size();

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

	// STEP 3: Move the camera in a spiral manner to scan for worms over the whole plate

	// enable multiple worm tracking
	worms.trackMultipleWorm = true;
	// vector that record the worms' postions relative to plate center (scale: pixel length)
	vector<Point> worms_positions;
	vector<double> worms_Area;

	for (int i = 0; i < (plate_map.getScanCoordPair()).size(); i++) { // iterate through all the entries of PlateMap

		// Go to the scan point
		if (!Ox.goToAbsolute((plate_map.getScanCoordPair()[i]).first, true, 25)) {
			// set the tilemap entry as UNSEARCHED if fail to scan that FOV
			plate_map.UpdatetileMap((plate_map.getScanCoordPair()[i]).second, PlateMap::UNSEARCHED);
			continue;
		}


		// TODO - change below to use ParalWormFinder instead of old method.
		// Wait for several seconds to let diff image settle down
		worms.worm_finding_mode = 0;
		boost_sleep_ms(8000);
		worms.worm_finding_mode = 1;
		boost_sleep_ms(5000);

		// To see whether have worms in the FOV
		//int num_worms = worms.HighEnergyCentroids.size();
		vector<Worm> found_worms = worms.all_worms;
		vector<Point> worm_centroids;

		for (Worm& w: found_worms) {
			worm_centroids.push_back(w.getWormCentroid());
		}

		for (int j = 0; j <found_worms.size(); j++) {
			cout << "Area " << "[" << j << "]" << " = " << found_worms[j].getArea() << endl;
		}

		//vector<Point> worms_loc = { worms.getPickupLocation() };
		int num_worms = found_worms.size();
		if (num_worms > 0) {
			// Set the tilemap entry as POPULATED
			plate_map.UpdatetileMap((plate_map.getScanCoordPair()[i]).second, PlateMap::POPULATED);

			// Convert the worms' coords to whole plates pixel coordinate
			cvtPlateCoord(worm_centroids, FOV_size, (plate_map.getScanCoordPair()[i]).second);

			// Append the worms' position & area to the vector
			for (int j = 0; j < found_worms.size(); j++) { 
				worms_positions.push_back(worm_centroids[j]);
				worms_Area.push_back(found_worms[j].getArea());
			}


			// If the scan mode is FIND_WORM: Stop once a POPULATED Region is found
			if (sc_mode == PlateMap::FIND_WORM) { cout << "Worm found!" << endl; oxLock.unlock(); return true; }
		}
		else {
			// Set the tilemap entry as UNPOPULATED
			plate_map.UpdatetileMap((plate_map.getScanCoordPair()[i]).second, PlateMap::UNPOPULATED);
		}
	}

	// disable multiple worm tracking
	worms.trackMultipleWorm = false;

	// If scan mode is FIND_WORM: when reach this point, it means no worm found on the plate
	if ( sc_mode == PlateMap::FIND_WORM) {
		cout << "No worm found!" << endl;
		oxLock.unlock(); 
		return false;
	}

	// STEP 4: Plot the status of FOVs
	plate_map.showPlateMap(worms_positions, worms_Area, FOV_size, Size(480, 480));

	// STEP 5: Go to the region that contains worms (If multiple regions contain worms, go to the one closest to the center)
	Point3d Pt_to_go = plate_map.getRegionContainWorm();
	if (Pt_to_go != Point3d(999, 999, 999)) {
		if (!Ox.goToAbsolute(Pt_to_go, true, 25)) {
			std::cout << "Fail to go to the POPULATED region" <<endl;
		}
	}

	//boost_sleep_ms(7000);
	//worms.findWorminWholeImg = false; // Enable tracking mode
	
	// Unlcok CNC
	oxLock.unlock();

	// TO DO: For demonstration: centering each the worm according to the plate coordinate
	return success;
}


// Find all the worms in the FOV (assuming the camera has been placed above the ROI already)
// Send the output the number of worms and their position in the image back to API socket
// No need to return data above inside C++ right now, since these data can be visible through worms object.
// Can change later whenever it need it.

bool executeFindWorm(WormPick& Pk, PlateTray& Tr, OxCnc& Ox, OxCnc& Grbl2, StrictLock<OxCnc>& oxLock, StrictLock<OxCnc>& grbl2Lock, WormFinder &worms, CameraFactory& camFact, WormSocket* sock_ptr){

	boost_sleep_ms(1000);
	worms.image_timer.setToZero();
	worms.worm_finding_mode = worms.FIND_ALL;

	// Wait for the worm finder thread to finish finding the worms
	// While we're waiting we can check to see if the pick is done heating
	//AnthonysTimer finding_timer;
	while (worms.worm_finding_mode != worms.WAIT) {

		boost_sleep_ms(50);
		boost_interrupt_point();
	}

	cout << "Worms list size: " << worms.all_worms.size() << endl;

	// If sock_ptr is pointing to a socket, then implying that script is running through the socket
	if (sock_ptr != nullptr) {

		string message = "";

		message.append("num\t" + to_string(worms.all_worms.size()) + "\n");

		for (int i = 0; i < worms.all_worms.size(); i++) {
			Point p = worms.all_worms[i].getWormCentroidGlobal();
			message.append("x\t" + to_string(p.x) + "\ty\t" + to_string(p.y) + "\n");
		}

		sock_ptr->send_data(message);
	}

	return true;
}

template <class  A>
float Mean(vector<A> vec) {

	float sum = 0.0;

	if (vec.size() == 0) { cout << "Cannot compute Mean (empty vector)" << endl;  return sum; }

	for (int i = 0; i < vec.size(); i++) {
		sum += vec[i];
	}
	return (sum / vec.size());
}

template <class B>
float StdDev(vector<B> vec) {
	float variance = 0.0;
	float SD;
	float mean = Mean(vec);

	for (int i = 0; i < vec.size(); i++) {
		variance += pow(vec[i] - mean, 2);
	}
	variance = variance / vec.size();
	SD = sqrt(variance);
	return SD;

}







// Adapted from https://stackoverflow.com/questions/10420454/shift-like-matlab-function-rows-or-columns-of-a-matrix-in-opencv
// Put to a seperated folder later
//circular shift one row from up to down
void shiftRows(Mat& mat) {

	Mat temp;
	Mat m;
	int k = (mat.rows - 1);
	mat.row(k).copyTo(temp);
	for (; k > 0; k--) {
		m = mat.row(k);
		mat.row(k - 1).copyTo(m);
	}
	m = mat.row(0);
	temp.copyTo(m);
}

bool executeAutoFocus(OxCnc &Ox, WormFinder& worms, CameraFactory& camFact, PlateTray& Tr) {
	// Note that the plates should still be focused prior to beginning an experiment. This is just meant to correct for any small focuse height
	// differences that occur across the plate. If the plate is far out of focus this method will take much longer (potentially losing worm tracking)
	// or might even not work at all. 

	cout << "Current Z height: " << Tr.getCurrentPosition().z << endl;


	// Turn off wormfinder
	int old_worm_finding_mode = worms.worm_finding_mode;
	worms.worm_finding_mode = worms.WAIT;

	// Enable laser imaging
	ImagingMode(LASER_AUTOFOCUS, Ox, camFact);

	// ROI for laser tracking.
	Size low_mag_size = worms.imggray.size();
	int width = low_mag_size.width * worms.laser_focus_roi_w_mult;
	int height = low_mag_size.height * worms.laser_focus_roi_h_mult;
	Rect low_mag_laser_rect = Rect(worms.high_mag_fov_center_loc.x - width / 2, worms.high_mag_fov_center_loc.y - height / 2, width, height);

	Size high_mag_size = worms.img_high_mag.size();

	/*shared_ptr<AbstractCamera> low_mag_cam = camFact.firstCamera();
	shared_ptr<StrictLock<AbstractCamera>> mainCamLock = camFact.getCameraLock(low_mag_cam);
	Mat wait_mat;
	cout << "Pre change exposure: " << endl;
	low_mag_cam->getExposureTime(*mainCamLock);*/

	AnthonysTimer focus_timer;
	double max_focus_time = 10;
	int distance_start_fine_tune = 5;

	int distance_use_small_step_sz = distance_start_fine_tune * 3; // start to use small step size while approaching focus
	
	bool start_use_small_step_sz = false;
	double small_step_sz = 0.05;
	double large_step_sz = 0.15;

	bool start_fine_tune = false;
	int focus_col_low_mag, focus_col_high_mag = 0;
	int dist_to_focus_low_mag, dist_to_focus_high_mag = 9999;

	int thresh_dist_to_focus = 50; // Threshold to say the laser line is in the center of high-mag 

	bool focused = false;
	worms.laser_focus_display = true; // to display the ROI for laser tracking. Note as of now the ROI is hard coded to display, so if you change it here you must also change it in WPHelper
	int last_framenum = WP_main_loop_framenum; // This will allow us to synchronize our movements with the camera images (which are updated in the main loop)
	while (!focused && focus_timer.getElapsedTime() < max_focus_time) {


		// Croase tune in low-mag image
		if (!start_fine_tune) {
			// Get the small subsection of the image with the laser (note that the image must be at least a little focused or else the laser may be outside of the roi, which would prevent the auto focus from working)
			Mat low_mag_laser_roi(worms.imggray(low_mag_laser_rect));

			// Calculate the intensity profile of the laser in the x direction and then get the column with maximum intensity (the peak of the laser)
			focus_col_low_mag = getCliffOfIntensityDistribution(low_mag_laser_roi) + (worms.high_mag_fov_center_loc.x - width / 2);

			dist_to_focus_low_mag = focus_col_low_mag - (worms.high_mag_fov_center_loc.x + worms.laser_focus_low_mag_offset);

			//cout << "dist_to_focus_low_mag = " << dist_to_focus_low_mag << endl;

			// If approaching the precalibrated point, start to use small step size
			if (abs(dist_to_focus_low_mag) < distance_use_small_step_sz) start_use_small_step_sz = true;

			// If approaching the precalibrated point, turn the flag to be true, indicating starting fine tune in hi-mag image
			if (abs(dist_to_focus_low_mag) < distance_start_fine_tune) start_fine_tune = true;
		}
		// Fine tune in hi-mag image
		else {
			// Calculate the intensity profile of the laser in the x direction and then get the column with maximum intensity (the peak of the laser)

			cout << "Fine tuning in the high-mag image..." << endl;
			focus_col_high_mag = getPeakOrCenterOfMassOfIntensityDistribution(worms.img_high_mag, true);

			dist_to_focus_high_mag = focus_col_high_mag - (high_mag_size.width / 2 + worms.laser_focus_high_mag_offset);

			cout << "dist_to_focus_high_mag = " << dist_to_focus_high_mag << endl;

		}

		/*cout << "Time to get laser line loc: " << endl;
		focus_timer.printElapsedTime();
		cout << "Col with biggest intensity drop: " << focus_col_low_mag << endl;
		cout << "Distance to focus: " << dist_to_focus << endl;*/

		if (start_fine_tune && abs(dist_to_focus_high_mag) <= thresh_dist_to_focus) {
			focused = true;
			cout << "New Z height: " << Ox.getZ() << endl;
		}
		else {

			int direction = 0;

			if (start_fine_tune) {
				direction = dist_to_focus_high_mag > 0 ? 1 : -1;
				Ox.jogOx(0, 0, direction * small_step_sz);
				
			}
			else {
				direction = dist_to_focus_low_mag > 0 ? 1 : -1;
				if (start_use_small_step_sz) { Ox.jogOx(0, 0, direction * small_step_sz); }
				else { Ox.jogOx(0, 0, direction * large_step_sz); }
			}
		}

		// Synchronize this loop with the framerate of the main loop because the image we use to find the laser is only updated in the main loop
		while (last_framenum == WP_main_loop_framenum) {
			boost_sleep_ms(10);
		}
		last_framenum = WP_main_loop_framenum;

	}

	Tr.setPlateZ(Ox.getZ()); // Remember the Z height. 

	// Return to brightfield imaging mode
	ImagingMode(BRIGHTFIELD, Ox, camFact);
	worms.laser_focus_display = false;

	// Turn wormfinder back
	worms.worm_finding_mode = old_worm_finding_mode;

	//boost_sleep_ms(1000);
	return focused;
}

// Measure how many pixel shift in low-mag image coresponding to unit movement in OxCNC.
// The pixel shift is assayed by template matching operation performed to images before and after OxCNC movement.
// Note: This script action assumes there has been a clear definite pattern (e.g. a micrometer calibration slide with clean white background) in the camera FOV already.
bool executeCalibrateOxCnc(WormPick &Pk, const PlateTray& Tr, OxCnc& Ox, StrictLock<OxCnc>& oxLock, WormFinder& worms, OxCnc& Grbl2, CameraFactory& camFact) {

	//// Verify that exactly one object is present. If not user should move to / click it first.
	//if (WF.all_worms.size() != 1) {
	//	std::cout << WF.all_worms.size() << " blobs visible in field (required: 1) - exiting" << endl;
	//	return false;
	//}

	//// Get the centroid of the blob
	//Point pt0 = WF.all_worms[0].getWormCentroid(); // PeteQ: Should this be the local coords or the global coords?
	//std::cout << "Target at: (" << pt0.x << "," << pt0.y << ")" << endl;

	ImagingMode(BRIGHTFIELD, Grbl2, camFact);

	// Get the input argument for calibrating OxCNC
	// Movement of stage
	double total_movement = stod(Tr.getCurrentScriptStepInfo().args[0]);
	// Whether to show images
	bool show_img = stoi(Tr.getCurrentScriptStepInfo().args[1]) != 0;

	//// args[2] = 0, calibrate x direction, args[2] = 1, calibrate y direction
	//bool calib_y = Tr.getCurrentScriptStepInfo().args[2];
	//bool calib_x = !calib_y;


	// Get the original position of stage
	Point3d Initial_pos = Ox.getXYZ();

	// Move in increments of 1
	Ox.goToRelative(0, -total_movement/2, 0, 1.0, true);
	boost_sleep_ms(1000);

	//// Verify that exactly one object is present. If not user should click it first.
	//if (WF.all_worms.size() != 1) {
	//	std::cout << WF.all_worms.size() << " blobs visible in field (required: 1) - exiting" << endl;
	//	return false;
	//}

	/*Point pty1 = WF.all_worms[0].getWormCentroid();
	std::cout << "Calibration starts...." << endl << "Starting Point: (" << pty1.x << " , " << pty1.y << ")" << endl;*/


	// Grab a image before stage movement
	Mat initial_img;
	worms.imggray.copyTo(initial_img);

	// Move the stage
	Ox.goToRelative(0, total_movement, 0, 1.0, true);
	boost_sleep_ms(1000);

	// Grab another image after stage movement
	Mat final_img;
	worms.imggray.copyTo(final_img);


	Rect display_ROI((final_img.cols)*0.35, (final_img.rows)*0.35, (final_img.cols)*0.3, (final_img.rows)*0.3);

	Mat img_referenced = initial_img(display_ROI);
	Mat img_operated = final_img(display_ROI);


	if (show_img) {
		imshow("Before moving (Ref)", img_referenced);
		waitKey(5000);
		imshow("After moving", img_operated);
		waitKey(5000);
	}

	Mat high_corr_img;
	img_operated.copyTo(high_corr_img);

	vector<Mat> results;
	// Calculate the correlation change vs. pixel shift
	for (int i = 0; i < img_operated.rows; i++) {

		Mat result;
		matchTemplate(img_referenced, img_operated, result, TM_CCOEFF_NORMED);
		//cout << "Corrcoef for shifting " << i << " pixel = " << result.at<float>(0, 0) << endl;
		//cout << "Result size: " << result.size().height << ", " << result.size().width << endl;
		results.push_back(result);

		// Shift one row
		shiftRows(img_operated);

	}


	int max_idx = -1;
	float max_correlation = -999999;
	for (int i = 0; i < results.size(); i++) {

		if (results[i].at<float>(0,0) > max_correlation) {
			max_correlation = results[i].at<float>(0, 0);
			max_idx = i;
		}
	}
	cout << "Max correlation = " << max_correlation << endl;


	//// Verify that exactly one object is present. If not user should click it first.
	//if (WF.all_worms.size() != 1) {
	//	std::cout << WF.all_worms.size() << " blobs visible in field (required: 1) - exiting" << endl;
	//	return false;
	//}

	/*Point ptym1 = WF.all_worms[0].getWormCentroid();
	std::cout << "Ending Point: (" << ptym1.x << " , " << ptym1.y << ")" << endl;*/

	double dy = max_idx;		/// How much y changed in the image
	double dOx = total_movement;					/// For Ox decreasing by -2 units
	double dydOx = dy / dOx;

	cout << "dy = " << dy << endl;
	cout << "dOx = " << dOx << endl;
	std::cout << "dy/dOx = " << dydOx << endl;
	Pk.dydOx = dydOx;

	for (int i = 0; i < max_idx; i++) {

		shiftRows(high_corr_img);

	}

	imshow("Highest correlated image", high_corr_img);
	waitKey(5000);


	//Move back to the initial position after the script action
	Ox.goToAbsolute(Initial_pos);

	return true;

}


// Calibrate the pick position in the FOV of machine vision camera
// Mapping the pick tip position with the coordinates of servo motors
bool executeCalibratePick(PlateTray& Tr, WormPick &Pk, OxCnc& Grbl2, StrictLock<OxCnc>& oxLock, WormFinder& worms) {
	
	// Start position of calibration range, cooresponding to the right upper corner

	// Length of calibration range
	int range = stoi(Tr.getCurrentScriptStepInfo().args[0]);
	// Step size of servo movement
	int step_sz = stoi(Tr.getCurrentScriptStepInfo().args[1]);

	// Calibration
	bool success = Pk.calibratePick(Grbl2, range, step_sz);

	return success;
}

// Calibrate the pincushion distortion of the low-magnification camera
// Save the distortion parameters to config file, help to undisort the image afterwards
bool executeCalibrateCam(CameraFactory& camFact) {

	shared_ptr<AbstractCamera> lowMag = camFact.Nth_camera(0);
	shared_ptr<StrictLock<AbstractCamera>> mainCamLock = camFact.getCameraLock(lowMag);

	// Calibrate distortion
	lowMag -> CalibrateDistortion(*mainCamLock);




	return true;
}




/*

	Helper functions

*/

// Control illuminators and cameras for different imaging mode
// enum imaging_mode { BRIGHTFIELD = 0, LASER_AUTOFOCUS = 1, BARCODE_READING = 2, GFP_IMAGING = 3, RFP_IMAGING = 4};
// Note that the low mag exposure options only range from -1 to -14. This is due to OpenCV and otherwise it will not work. 
// TODO: write uitility for reading status of each IO for each imaging mode from json. Rather than manually tune the IO
void ImagingMode(imaging_mode mode, OxCnc &Grbl2, CameraFactory& camFact) {

	// FIO7 control the ON/OFF status of white illumination;
	// EIO5 (FIO13) control the ON/OFF status of blue LED
	// EIO4 (FIO12)	control the ON/OFF status of green LED

	Config::getInstance().loadFiles();
	json imaging_mode_config = Config::getInstance().getConfigTree("imaging_mode");


	shared_ptr<AbstractCamera> lowMag = camFact.Nth_camera(0);
	shared_ptr<StrictLock<AbstractCamera>> mainCamLock = camFact.getCameraLock(lowMag);

	// Get the pointer to access the fluo camera
	shared_ptr<AbstractCamera> highMag = camFact.Nth_camera(2);
	shared_ptr<StrictLock<AbstractCamera>> fluoCamLock = camFact.getCameraLock(highMag);

	// Get the current exposure time
	//long long origl_expo = highMag->getExposureTime(*fluoCamLock);

	if (mode == BRIGHTFIELD) {

		// Get the exposure from config
		double lowMag_expo = imaging_mode_config["BRIGHTFIELD"]["GENERIC_exposure"].get<double>();
		//cout << "lowMag_expo = " << imaging_mode_config["BRIGHTFIELD"]["GENERIC_exposure"].get<long long>() << endl;
		long long highMag_expo = imaging_mode_config["BRIGHTFIELD"]["THORLABS_exposure"].get<long long>();

		// Get the hardware delay from config
		long delay = imaging_mode_config["BRIGHTFIELD"]["Hardware_delay"].get<long>();

		// Reset the low mag exposure
		lowMag->setExposureTime(*mainCamLock, lowMag_expo);

		// Set exposure time to low value
		highMag->setExposureTime(*fluoCamLock, highMag_expo);

		// Turn the blue LED to Off
		LabJack_SetFioN(Grbl2.getLabJackHandle(), 13, 0);

		// Turn the green LED to Off
		LabJack_SetFioN(Grbl2.getLabJackHandle(), 12, 0);

		// Turn off laser
		LabJack_SetFioN(Grbl2.getLabJackHandle(), 15, 0);

		// Turn off side illumination
		LabJack_SetFioN(Grbl2.getLabJackHandle(), 10, 0);

		// Turn the white illumination to ON
		LabJack_SetFioN(Grbl2.getLabJackHandle(), 7, 1);

		// Add some wait time to make sure the harware is ready
		boost_sleep_us(delay);

	}
	else if (mode == LASER_AUTOFOCUS) {

		// Get the exposure from config
		double lowMag_expo = imaging_mode_config["LASER_AUTOFOCUS"]["GENERIC_exposure"].get<double>();
		long long highMag_expo = imaging_mode_config["LASER_AUTOFOCUS"]["THORLABS_exposure"].get<long long>();
		long delay = imaging_mode_config["LASER_AUTOFOCUS"]["Hardware_delay"].get<long>();

		// Turn on laser
		LabJack_SetFioN(Grbl2.getLabJackHandle(), 15, 1);
		
		// Turn the white illumination to OFF
		LabJack_SetFioN(Grbl2.getLabJackHandle(), 7, 0);

		// Set low mag exposure to high value
		lowMag->setExposureTime(*mainCamLock, lowMag_expo);

		// Set exposure time to high value
		highMag->setExposureTime(*fluoCamLock, highMag_expo);

		// Add some wait time to make sure the frame has been in the fluo mode
		boost_sleep_us(delay);

	}
	else if (mode == BARCODE_READING) {
		// Get the hardware delay from config
		long delay = imaging_mode_config["BARCODE_READING"]["Hardware_delay"].get<long>();

		// Turn off brightfield
		LabJack_SetFioN(Grbl2.getLabJackHandle(), 7, 0);

		// Turn on sideillumination
		LabJack_SetFioN(Grbl2.getLabJackHandle(), 10, 1);

		// Add some wait time to make sure the harware is ready
		boost_sleep_us(delay);
	}
	else if (mode == GFP_IMAGING) {

		// Turn the blue LED to Off
		LabJack_SetFioN(Grbl2.getLabJackHandle(), 12, 0);

		// Turn the white illumination to OFF
		LabJack_SetFioN(Grbl2.getLabJackHandle(), 7, 0);

		// Get the exposure from config
		long long highMag_expo = imaging_mode_config["GFP_IMAGING"]["THORLABS_exposure"].get<long long>();

		// Get the hardware delay from config
		long delay = imaging_mode_config["GFP_IMAGING"]["Hardware_delay"].get<long>();

		// Set exposure time to high value
		highMag->setExposureTime(*fluoCamLock, highMag_expo);

		// Turn the blue LED to ON
		LabJack_SetFioN(Grbl2.getLabJackHandle(), 13, 1);


		// Add some wait time to make sure the frame has been in the fluo mode
		boost_sleep_us(delay + highMag_expo);

	}
	else if (mode == RFP_IMAGING) {

		// Turn the blue LED to Off
		LabJack_SetFioN(Grbl2.getLabJackHandle(), 13, 0);

		// Turn the white illumination to OFF
		LabJack_SetFioN(Grbl2.getLabJackHandle(), 7, 0);

		// Get the exposure from config
		long long highMag_expo = imaging_mode_config["RFP_IMAGING"]["THORLABS_exposure"].get<long long>();

		// Get the hardware delay from config
		long delay = imaging_mode_config["RFP_IMAGING"]["Hardware_delay"].get<long>();

		// Set exposure time to high value
		highMag->setExposureTime(*fluoCamLock, highMag_expo);

		// Turn the green LED to ON
		LabJack_SetFioN(Grbl2.getLabJackHandle(), 12, 1);

		// Add some wait time to make sure the frame has been in the fluo mode
		boost_sleep_us(delay + highMag_expo);
	}

	return void();
}


void cvtPlateCoord(std::vector<Point>& coord, cv::Size FOV_size, MatIndex idx) {

	for (int i = 0; i < coord.size(); i++) {
		coord[i].x = coord[i].x + (idx.col) * FOV_size.width;
		coord[i].y = coord[i].y + (idx.row) * FOV_size.height;
	}
}

string getPlateIdKey(PlateTray& Tr) {
	int r = Tr.getCurrentScriptStepInfo().id.row;
	int c = Tr.getCurrentScriptStepInfo().id.col;
	return to_string(r) + to_string(c);
}

// Calculate the absolute coordinates of worms' position
vector<Point3d> convertCoord(vector<Point> FOV_posit, Point center_coord, Point3d stage_coord, double dydOx) {

	vector<Point3d> abs_coord;

	for (int i = 0; i < FOV_posit.size(); i++) {

		Point3d this_pt;
		this_pt.x = stage_coord.x + (FOV_posit[i].x - center_coord.x) / dydOx;
		this_pt.y = stage_coord.y + (FOV_posit[i].y - center_coord.y) / dydOx;
		this_pt.z = stage_coord.z;

		abs_coord.push_back(this_pt);
	}

	return abs_coord;

}

bool executePlatePosRel(PlateTray& Tr) {
	Tr.currentOffset.x += stod(Tr.getCurrentScriptStepInfo().args[0]);
	Tr.currentOffset.y += stod(Tr.getCurrentScriptStepInfo().args[1]);
	Tr.currentOffset.z += stod(Tr.getCurrentScriptStepInfo().args[2]);
	return true;
}

bool performIntermediatePickingStep(WormPick& Pk, PlateTray& Tr, OxCnc& Ox, OxCnc &Grbl2, StrictLock<OxCnc>& oxLock, StrictLock<OxCnc> &grbl2Lock, 
	WormFinder& worms, CameraFactory& camFact, int& kg, int& num_worms, MatIndex int_loc, vector<Phenotype>& worm_phenotypes_found)
{
	//experiment_data << "\tHowever, " << num_worms << " worms were found in high mag view. Attempting an intermediate pick." << endl;
	// We have multiple worms in the high mag FOV so perform an intermediate picking step to isolate the worm we want
	// Move to intermediate plate
	cout << "============= Moving to Intermediate ==============" << endl;
	//PlateTray dummy_tray(Tr);
	Tr.changeScriptId(int_loc);
	Tr.updateStepInfo();
	Tr.changeScriptArgs(MOVE_TO_DESTINATION_DEFAULT); // set move inputs
	Rect custom_move_offset_region(-3, -8, 10, 16); // reduce the move offset region so we're more likely to drop on the bacteria.
	move_offset_region = &custom_move_offset_region;
	executeMoveToPlate(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);

	//experiment_data << "\tAt intermediate plate." << endl;

	// Drop the worm at the Intermediate plate
	cout << "============= Dropping worm at intermediate ==============" << endl;
	worms.overlayflag = false;
	int orig_num_worms = num_worms;
	cout << "Original number of worms and num_worms before speed pick: " << orig_num_worms << ", " << num_worms << endl;
	Tr.changeScriptArgs(DROP_OFF_WORMS_DEFAULT); // set pickworm inputs
	bool drop_success = executeSpeedPick(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact, &num_worms);
	worms.overlayflag = true;

	cout << "Original number of worms and num_worms after speed pick: " << orig_num_worms << ", " << num_worms << endl;
	cout << "Drop success: " << drop_success << endl;
	if (!drop_success && orig_num_worms == num_worms) {
		// failed to drop any worms off of the pick
		cout << "Failed to drop any worms to the intermediate plate. Abandoning intermediate pick." << endl;
		//experiment_data << "\tFailed to drop any worms to the intermediate plate. Abandoning intermediate pick." << endl;
		return false;
	}

	Pk.heatPickInThread(Grbl2, Pk, 0, 2, 0.2); // Sterilize the pick

	boost_sleep_ms(2000); // Wait a couple seconds to allow the worms to crawl away from one another

	// Find and center a worm to pick
	cout << "============= Finding and centering a worm ==============" << endl;
	worm_phenotypes_found.clear();
	int num_tries = 0;
	int max_tries = 4;
	//Tr.changeScriptArgs(CENTER_WORMS_DEFAULT); // set centerworm inputs
	bool success = false;
	do {
		cout << "Attempting to find and center a worm for picking." << endl;
		//experiment_data << "\tAttempt " << num_tries + 1 << " of " << max_tries << " to find and center a worm for picking." << endl;
		Tr.changeScriptArgs(CENTER_WORMS_DEFAULT); // set centerworm inputs
		success = executeCenterWorm(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact, kg, worm_phenotypes_found, num_worms);
		num_tries++;
		if (!success) {
			//experiment_data << "\tFailed to find and center a worm for picking." << endl;
			cout << "\tFailed to find and center a worm for picking." << endl;
			if (num_worms > 0) {
				if (!worms.unknown_pheno_in_high_mag)
				{
					//cout << num_worms << " undesirable worms in high mag view. Picking them and heating pick to remove them from intermediate plate." << endl;
					////experiment_data << "\t" << num_worms << " undesirable worms in high mag view. Picking them and heating pick to remove them from intermediate plate." << endl;
					//Tr.changeScriptArgs(PICK_UP_WORMS_DEFAULT); // set pickworm inputs
					//executeSpeedPick(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);
					//Pk.heatPickInThread(Grbl2, Pk, 0, 2, 0.2);
				}
				else {
					cout << "At least 1 worm in high mag has unknown properties for the desired phenotypes. So we won't perform burning step because it could potentially be a worm we want." << endl;
				}
			}
		}

	} while (!success && num_tries < max_tries);

	if (!success) {
		// failed to find a worm with matching phenotype
		//experiment_data << "\tFailed to find a worm with the matching phenotype. Abandoning intermediate pick." << endl;
		cout << "\tFailed to find a worm with the matching phenotype. Abandoning intermediate pick." << endl;
		return false;
	}

	//experiment_data << "\tWorm has been selected for picking." << endl;
	cout << "\tWorm has been selected for picking." << endl;
	cout << "============= Picking a worm ==============" << endl;
	Tr.changeScriptArgs(PICK_UP_WORMS_DEFAULT); // set pickworm inputs
	executeSpeedPick(Pk, Tr, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact);

	return true;
}

// A helper function that obtain valid args from user input
// arg_input: reference of an object to hold the user input
// possible_valid_args: reference of a vector listing all the possible valid input args. If empty, then no checking will be performed
template<class T>
void InputValidArgs(T& arg_input, const std::vector<T>& possible_valid_args) {
	while (true) {
		cin >> arg_input;
		try {
			if (cin.fail()) throw "Invalid Entry";

			// If possible_valid_args is a null pointer, then no checking to perform
			if (possible_valid_args.size() == 0) break;

			if (vectorFind(possible_valid_args, arg_input) != -1) {
				// break if the index_returned is within the possible list
				break;
			}
			else {
				throw "Invalid Entry";
			}
		}
		catch (...) {
			cout << "Invalid entry. Please enter again." << endl;
			cin.clear();
			cin.ignore();
		}
	}
}


vector<Phenotype> getWantedPhenotypeFromInput(int num_source_plates, vector<int>* num_worms_from_sources) {

	vector<Phenotype> phenotypes; // Maps the plate number to the phenotyping we will do on that plate.
	
	// Get the number of worms to pick from each source plate
	if (num_worms_from_sources) {
		// Get the number of worms to pick from each source plate
		for (int i = 0; i < num_source_plates; i++) {
			cout << "Please enter the number of worms to pick from source plate " << i + 1 << "." << endl;
			int num_to_pick = -1;
			InputValidArgs(num_to_pick);
			(*num_worms_from_sources).push_back(num_to_pick);
		}
	}
	 
	// Get the desired phenotypes to pick from each source plate
	for (int i = 1; i <= num_source_plates; i++) {

		// Construct the phenotyping method from user input
		cout << "Please enter the numbers, one at a time, of the phenotype you would like to pick on source plate " << i << "." << endl;

		int sex_type_wanted = -1;
		int gfp_type_wanted = -1;
		int rfp_type_wanted = -1;
		int morphology_type_wanted = -1;
		int stage_type_wanted = -1;

		// Sex
		cout << "\t0: No Sex Preference" << endl;
		cout << "\t1: Pick Only Herms" << endl;
		cout << "\t2: Pick Only Males" << endl;
		InputValidArgs(sex_type_wanted, { 0,1,2 });

		// GFP
		cout << "\t0: No GFP Preference" << endl;
		cout << "\t1: Pick Only GFP Worms" << endl;
		cout << "\t2: Pick Only Non-GFP Worms" << endl;
		InputValidArgs(gfp_type_wanted, { 0,1,2 });

		// RFP
		cout << "\t0: No RFP Preference" << endl;
		cout << "\t1: Pick Only RFP Worms" << endl;
		cout << "\t2: Pick Only Non-RFP Worms" << endl;
		InputValidArgs(rfp_type_wanted, { 0,1,2 });

		// Morphology
		cout << "\t0: No Morphology Preference" << endl;
		cout << "\t1: Pick Only Normal Morphology Worms" << endl;
		cout << "\t2: Pick Only Dumpy Worms" << endl;
		InputValidArgs(morphology_type_wanted, { 0,1,2 });

		// Stage
		cout << "\t0: No Developmental Stage Preference" << endl;
		cout << "\t1: Pick Only Adult Worms" << endl;
		cout << "\t2: Pick Only L4 Worms" << endl;
		cout << "\t3: Pick Only L3 larva Worms" << endl;
		cout << "\t4: Pick Only L2 larva Worms" << endl;
		cout << "\t5: Pick Only L1 larva Worms" << endl;
		InputValidArgs(stage_type_wanted, { 0,1,2,3,4,5 });

		// Construct the phenotype object
		Phenotype phenotype_wanted(
			(sex_types)sex_type_wanted,
			(GFP_types)gfp_type_wanted,
			(RFP_types)rfp_type_wanted,
			(morph_types)morphology_type_wanted,
			(stage_types)stage_type_wanted
		);

		// Append the phenotyping method to the return list
		phenotypes.push_back(phenotype_wanted);

	}

	return phenotypes;
}


// This is a helper function for high level scripts that don't necessarily have the script arguments available to detail the desired phenotyping during an experiment
// You can first run getPhenotypeParameters to have the user enter their desired phenotyping for each source plate. Then each time you move to a new plate you can set the 
// current_phenotyping_desired member variable and run this to set the appropriate script arguments and run the phenotyping.
//bool setAndRunPhenotyping(WormPick &Pk, const PlateTray& Tr, OxCnc& Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder& worms, CameraFactory& camFact, int& num_worms) {
//	// TODO: Do error checking for contradictory phenotypes, like if user requests both herm and male, or L2_LARVA male, etc...
//	// TODO: Add a way to allow multiple stages to be selected?? i.e. pick L4_LARVA OR adults, but not L1_LARVA-3.
//	PlateTray dummy_tray(Tr);
//
//	double sex_phenotype = static_cast<int>(current_phenotyping_method.check_sex_option);
//	double GFP_phenotype = static_cast<int>(current_phenotyping_method.check_GFP_option);
//	double RFP_phenotype = static_cast<int>(current_phenotyping_method.check_RFP_option);
//	double morphology = static_cast<int>(current_phenotyping_method.check_morphology_option);;
//	double stage = static_cast<int>(current_phenotyping_method.check_stage_option);
//
//	// Now set the script arguments based on the desired phenotyping we found
//	dummy_tray.changeScriptArgs({ sex_phenotype, GFP_phenotype, RFP_phenotype, morphology, stage, 0, 0, 0, 1, 1 });
//	cout << "Running phenotype... " << endl;
//
//	return executePhenotype(Pk, dummy_tray, Ox, Grbl2, oxLock, grbl2Lock, worms, camFact, num_worms);
//}

bool centerTrackedWormInHighMag(OxCnc& Ox, WormFinder& worms, WormPick& Pk, const PlateTray& Tr, bool wait_if_lost, bool use_mask_rcnn) {
	Point tracked_centroid = worms.getTrackedCentroid();
	
	// Get the offset to the worm in OxCnc units so we know how far we need to move the robot
	double dx = (tracked_centroid.x - worms.high_mag_fov_center_loc.x) / Pk.dydOx;
	double dy = (tracked_centroid.y - worms.high_mag_fov_center_loc.y) / Pk.dydOx;

	//cout << "dx: " << dx << ", dy: " << dy << endl;

	int old_worm_mode = worms.worm_finding_mode;
	worms.worm_finding_mode = worms.WAIT;
	boost_sleep_ms(100);

	cout << "Moving to tracked worm." << endl;
	Ox.goToRelative(dx, dy, 0, 1.0, true);
	worms.updateWormsAfterStageMove(dx, dy, Pk.dydOx);

	worms.updatePickableRegion(Ox.getXYZ(), Tr.getCurrentPosition(), Pk.dydOx);
	cout << "New pickable region from center high mag: " << worms.pickable_region.pickable_center << endl;
	
	//cout << "Targeted worm current pos: " << tracked_worm.getWormCentroidGlobal().x << ", " << tracked_worm.getWormCentroidGlobal().y << endl;
	boost_sleep_ms(500);
	worms.worm_finding_mode = old_worm_mode;
	
	//focusImage(Pk, Ox, worms, 0, 0); // Focus the high mag image. 

	// TODO need to jump to worm again after focus?

	//boost_sleep_ms(4000);
	//waitKey(0);

	return centerTrackedWormToPoint(worms, Ox, worms.high_mag_fov_center_loc, worms.TRACK_ONE, wait_if_lost, use_mask_rcnn);
}


//TODO: DELETE THIS AFTER CLEANING UP THE REST OF THE PERTAINING CODE
//void handleLidGrabbing(PlateTray& Tr, OxCnc& Ox) {
//	boost::numeric::ublas::matrix<boost::optional<plate>> plateMat = Tr.getPlateMat();
//	MatIndex plate_id = Tr.getCurrentScriptStepInfo().id;
//
//	// Check if the plate has already had its plate grabbed
//	if (plateMat(plate_id.row, plate_id.col)->is_grabbed) {
//		return; // No need to do anything because the lid is currently lifted
//	}
//
//	// Get the plate coordinates of the plate lids we are currently holding
//	vector<MatIndex> handled_lids;
//	for (auto i = 0; i < plateMat.size1(); i++) {
//		for (auto j = 0; j < plateMat.size2(); j++) {
//			if (plateMat(i, j)->is_grabbed) {
//				handled_lids.push_back(MatIndex(i, j));
//			}
//		}
//	}
//
//	int num_lid_grabbers = 2;
//	int grabber_to_use = 0;
//	if (handled_lids.size() < num_lid_grabbers) {
//		grabber_to_use = handled_lids.size() + 1;
//	}
//	else {
//		for (int i = 0; i < handled_lids.size(); i++) {
//			if (plateMat(handled_lids[i].row, handled_lids[i].col)->is_source == plateMat(plate_id.row, plate_id.col)->is_source) {
//
//			}
//		}
//	}
//
//	
//
//}

// Helper function to auto-focus the image e.g. before picking
bool focusImage(WormPick& Pk, OxCnc& Ox, WormFinder& worms, bool is_low_mag, bool move_pick_slightly_away) {

	// Ensure pick is clear
	if (move_pick_slightly_away) {
		Pk.movePickSlightlyAway();
	}
	else{
		Pk.movePickAway();
	}

	// Run focuser
	focus_image(Ox, Pk, worms, is_low_mag); // FocusImage (not focus_image) is not being used yet, and in a previous version of the software the last argument to focus_image 'isLowMag' was unneeded.
								   // I've set it to 1 (because of executeFocusImage) but I'm unsure if it should always be 1 or not for this function.
	// Store plate's focus height in PlateTray
	// TODO
	return true;
}

vector<MatIndex> generatePlateLocations(set<int> rows_with_plates, PlateTray& Tr, WormPick& Pk, vector<MatIndex> excluded_locations, LidHandler::plate_type plate_type) {
	// Create a vector of all the plates we can use by checking if they're valid locations
	vector<MatIndex> plate_locations;
	for (int row : rows_with_plates) {
		for (int i = 0; i < Tr.getNumColumns(); i++) {
			MatIndex plate_loc(row, i);
			if (Tr.validateStepInfo(plate_loc)) { // Check that it is a valid location (row/col in bounds of tray)
				bool is_excluded = false;
				for (MatIndex excluded_loc : excluded_locations) { // Check to see if this location should be excluded
					if (excluded_loc == plate_loc) {
						is_excluded = true;
						break;
					}
				}
				
				if (!is_excluded) {
					Pk.lid_handler.setPlateType(plate_loc, plate_type);
					plate_locations.push_back(plate_loc);
				}
			}
			else {
				cout << "Excluded plate location found: (" << plate_loc.row << ", " << plate_loc.col << "). Skipping this plate location." << endl;
			}
		}
	}

	return plate_locations;
}

void stimulateMovementWithBlueLight(WormFinder& worms, OxCnc &Grbl2, CameraFactory& camFact, int secs_to_stimulate) {

	int old_worm_finding_mode = worms.worm_finding_mode;
	worms.worm_finding_mode = worms.WAIT;
	// Enable fluorescence imaging
	ImagingMode(GFP_IMAGING, Grbl2, camFact);

	// Stimulate the worms for the specified amount of time.
	boost_sleep_ms(secs_to_stimulate * 1000);

	// Turn back to brightfield imaging
	ImagingMode(BRIGHTFIELD, Grbl2, camFact);
	worms.worm_finding_mode = old_worm_finding_mode;
}

bool clarifyDumpyStatus(WormFinder& worms, bool initial_dumpy_result) {
	// TODO: Do we need to track the worm in between trials? The more trials the longer this will take and the worm could crawl away.
	// Also, right now I am not waiting at all, but if we wait a little bit then the images will be less correlated which could lead to better results.

	int dumpys_found = initial_dumpy_result ? 1 : 0;
	int trials = 2; // Number of extra times to look at the worms to determine dumpiness. Should be even to prevent ties.

	for (int j = 0; j < trials; j++) {
		// Grab a new worm image
		Mat worm_img;
		worms.img_high_mag_color.copyTo(worm_img);

		Worm test_worm; // We don't want to change anything with the tracked worm so we will just make a new worm for this

		// Feed the image to mask-rcnn server and get results.
		cout << "Sending image to Python server" << endl;
		maskRCNN_results results = maskRcnnRemoteInference(worm_img);
		cout << "Results received. " << endl;

		// Get the number of worms in the returned mask
		int num_worms = results.num_worms;

		if (num_worms == 0) {
			continue;
		}

		// Because there can be multiple worms in the image we will look through all the worm masks and check if any of them are dumpy.
		// If a worm is dumpy then we can immediatley start the next trial, but if not then we need to keep looking at the rest of the worms.
		for (int i = 0; i < num_worms; i++) {
			// Segment the worm (this will set the physical characteristics of the worm - contours/centerline/sex/head-tail locations/morphology
			test_worm.PhenotypeWormFromMaskRCNNResults(worm_img, results, i, 30, 0.45, false);

			// Check if this worm is Dumpy
			if (test_worm.current_phenotype.MORPH_type == DUMPY) {

				// Save the mask for debugging
				Mat debug_mask;
				Mat debug_img;
				worm_img.copyTo(debug_img);
				results.worms_mask.copyTo(debug_mask);
				debug_mask *= 35;
				string datetime_string = getCurrentTimeString(false, true);
				string verif_img_fpath = "C:\\WormPicker\\recordings\\verification_images\\dumpy_classify\\" + datetime_string + "_dumpy_img.png";
				string verif_mask_fpath = "C:\\WormPicker\\recordings\\verification_images\\dumpy_classify\\" + datetime_string + "_dumpy_mask.png";
				vector<int> params{ CV_IMWRITE_PNG_COMPRESSION, 0 };
				imwrite(verif_img_fpath, debug_img, params);
				imwrite(verif_mask_fpath, debug_mask, params);

				dumpys_found++;
				break; 

			}
		}
	}
	return dumpys_found > (trials + 1)/2; // Add 1 to trials because we already did a trial before calling this function
}

// Thread function to cycle script steps and/or plates for the WWExperiment

void cycleWormScript(PlateTray &Tr, OxCnc &Ox, OxCnc& Grbl2,
	StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock,
	int* ptr_kg_or_rflag_Script_thread_isReady, WormPick& Pk,
	WormFinder& worms, CameraFactory& camFact)
{
	/*
		Depending on where to get the commands for script execution,
		the cycleWormScript thread configures the infinite loop differently.

		If commands come from inside the C++ (listen_to_Socket = false), the infite loop monitor the user key trigger 
		and execute the scripts specified in C:\WormPicker\params_PlateTray_ActionScript.txt

		If commands come from socket (listen_to_Socket = true), this thread setup an acceptor socket listening to another language, e.g. Python
		and the infite loop keep reading data from socket
		Socket server adapted from
		https://subscription.packtpub.com/book/application_development/9781783986545/1/ch01lvl1sec16/accepting-connections
	*/

	// Create plate tracker instance
	string dbFilename = "C:\\WormPicker\\Database\\exp1.csv";  // TODO: read from json
	PlateTracker plateTracker = PlateTracker(dbFilename);

	// If commands come from inside the C++
	if (Tr.getListen_to_Socket() == false) {

		for (;;) {

			// Check for exit request
			boost_sleep_ms(25);
			boost_interrupt_point();


			/* -------------- USER MANUAL REQUESTS via BUTTON PRESS ------------------- */

			// Check if manual Ox CNC  movement was requested
			if (!OxCnc::move_secondary_manual && Ox.isConnected() && Ox.isRecognizedKey(*ptr_kg_or_rflag_Script_thread_isReady)) {
				///oxLock.lock(); /// Currently unnecessary to lock since all write-access in cyclePlates... thread
				Ox.goToByKey(*ptr_kg_or_rflag_Script_thread_isReady, oxLock, true);
				///oxLock.unlock();
			}
			else if (OxCnc::move_secondary_manual && Grbl2.isConnected() && Grbl2.isRecognizedKey(*ptr_kg_or_rflag_Script_thread_isReady)) {
				///grbl2Lock.lock();
				Grbl2.goToByKey(*ptr_kg_or_rflag_Script_thread_isReady, grbl2Lock, true);
				///grbl2Lock.unlock();
			}

			// Check if manual DYNAMIXEL movement was requested
			Pk.movePickByKey(*ptr_kg_or_rflag_Script_thread_isReady, Grbl2);

			// Check whether the user requested that the pick be centered now
			if (*ptr_kg_or_rflag_Script_thread_isReady == keyAsterisk) {
				cout << "Centering pick...\n";
				Pk.centerPick(Ox, Grbl2, worms);
			}

			// Flush the keypress to ready for a new one.
			*ptr_kg_or_rflag_Script_thread_isReady = -5;


			/* --------------------- SCRIPT-DEFINED ACTIONS  ----------------------- */


			// Take no action if it's not time to run (or currently running). Note that red will be turned on prior to experiment

			if (!Tr.checkForStartTime())
				continue;


			// Get the plate name and output path
			Tr.updateCurrentPath(); /// Updates save_path
									//plate_name = Tr.getCurrentPlateName();

			// If it is time to run and we're just starting (e.g. on the first step), home the CNC first
			/*if (Tr.atFirstValidStep() && Ox.isConnected())
				Ox.homeCNC(false);*/

				// If the plate tray is in DUMMY mode, skip the actual experiment, otherwise run it
				//	Experiment is also forced to run if GRBL is not connected. Otherwise it runs through the script and does nothing.
			bool success = false;

			if (!Tr.dummy_mode || !Ox.isConnected()) {

				// Run script step
				success = executeScriptAction(Tr, Tr.getErr(), Pk, Ox, Grbl2, oxLock, grbl2Lock, worms, *ptr_kg_or_rflag_Script_thread_isReady, camFact, plateTracker) &&
					Tr.getPlateStateIsRunning();
				boost_sleep_ms(100);

				// Abort script if any step files
				if (!success) {
					cout << "Script stopped after step: " <<
						Tr.getCurrentScriptStepInfo().step_action << endl;
					Tr.resetPlateSeries();
				}
			}

			/*
				Move to the next step and/or plate in the script.
				- If this is the last step / plate step will be set back to 0.
				- If action did not complete successfully, the script has been reset and we shouldn't increment here.
				- If there was no plate at the requested position,
			*/
			if (success)
				Tr.setNextPlate();
		}
	
	
	}

	// If command come from socket
	else if (Tr.getListen_to_Socket() == true) {

		// Load parameters for set up socket and signal messages from config file 
		json execution_config = Config::getInstance().getConfigTree("API scripting")["script_execution"];
		string success_signal = execution_config["execution_result_signal"]["success"].get<string>();
		string fail_signal = execution_config["execution_result_signal"]["fail"].get<string>();


		// Set up a server socket for script execution
		string raw_ip_address = Tr.getRaw_ip_address();
		unsigned short port_num = Tr.getPort_num();

		Socket_Type s_type = SERVER;
		WormSocket script_sock(s_type, raw_ip_address, port_num);

		// Up to thid point, the thread is ready to receive commands from socket
		*ptr_kg_or_rflag_Script_thread_isReady = 1;

		// TODO: operate socket in non-blocking mode so that the accept can be called before turning the ready flag to true
		// Accept Connection
		script_sock.accept_connection();
		
		// Start keep reading from socket, trigger the script execution once commands come in

		string commands = "";

		for (;;) {
			cout << "C++ is waiting for commands from socket..." << endl;

			commands = script_sock.receive_data();

			Tr.saveMsgfromSocket(commands);

			// Prepare script based on commands received from the socket
			bool success_startPlateSeries = Tr.startPlateSeries();

			// Get the plate name and output path
			if (success_startPlateSeries) Tr.updateCurrentPath(); /// Updates save_path
									//plate_name = Tr.getCurrentPlateName();



			// If the plate tray is in DUMMY mode, skip the actual experiment, otherwise run it
				//	Experiment is also forced to run if GRBL is not connected. Otherwise it runs through the script and does nothing.
			bool success = false;

			if (!Tr.dummy_mode || !Ox.isConnected()) {


				if (success_startPlateSeries) {
					// Run script step
					success = executeScriptAction(Tr, Tr.getErr(), Pk, Ox, Grbl2, oxLock, grbl2Lock, worms, *ptr_kg_or_rflag_Script_thread_isReady, camFact, plateTracker, &script_sock) &&
						Tr.getPlateStateIsRunning();
					boost_sleep_ms(100);
				}

				// Abort script if any step files
				if (!success) {

					cout << fail_signal <<
						Tr.getCurrentScriptStepInfo().step_action << endl;

					if (success_startPlateSeries) Tr.resetPlateSeries();

					// Send the status of script execution to socket
					script_sock.send_data(fail_signal + Tr.getCurrentScriptStepInfo().step_action);
					
				}
			}

			/*
				Move to the next step and/or plate in the script.
				- If this is the last step / plate step will be set back to 0.
				- If action did not complete successfully, the script has been reset and we shouldn't increment here.
				- If there was no plate at the requested position,
			*/
			if (success) {
				Tr.setNextPlate();
				// Send the status of script execution to socket
				script_sock.send_data(success_signal + Tr.getCurrentScriptStepInfo().step_action);
			}

		}
		
	}

}