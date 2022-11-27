/*
	ScriptActions.h
	Anthony Fouad
	Fang-Yen Lab
	December 2018

	Library of low-level actions that can be requested by WormScript.

	executeScriptAction interperets the action from the plate tray's script object, likely from the script step,
	and passes along references to the hardware needed to complete the action to the corresponding
	action function.
*/
#pragma once
#ifndef SCRIPTACTIONS_H
#define SCRIPTACTIONS_H

// Anthony's includes
#include "WormPick.h"
#include "OxCnc.h"
#include "StrictLock.h"
#include "PlateTray.h"
#include "WormFinder.h"
#include "Worm.h"
#include "ErrorNotifier.h"
#include "PlateMap.h"
#include "CameraFactory.h"
#include "LidHandler.h"
#include "ClassifyDumpy.h"
#include "PlateTracker.h"
#include "WormSocket.h"
//#include "GenericCamera.h"

// Standard includes
#include <string>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <tuple>

//extern std::string fluo_img_dir;
extern std::string phenotype_img_dir;
extern std::string BF_img_dir;
extern std::string GFP_img_dir;
extern std::string RFP_img_dir;
extern std::string global_exp_title;
extern int WP_main_loop_framenum;

//enum phenotype_options { HERM_ONLY = 1, MALE_ONLY, GFP_ONLY, NON_GFP_ONLY, RFP_ONLY, NON_RFP_ONLY, NORMAL_MORPH, DUMPY, ADULT, L4_LARVA, L3_LARVA, L2_LARVA, L1_LARVA };


// Structure for handling phenotyping parameters
// Note: please keep the values of the enum be automatically assigned!
// The last element in the enum is the dummy counter to indicate the number of elements in the list, 
// help to reduce the hardcoded parameter whenever need to avoid invalid indexing
//enum check_sex_options { NOTCHECKSEX, HERM_WANTED, MALE_WANTED, DUMMY_COUNT_SEX_OPTION};
//enum check_GFP_options { NOTCHECKGFP, GFP_WANTED, NON_GFP_WANTED, DUMMY_COUNT_GFP_OPTION};
//enum check_RFP_options { NOTCHECKRFP, RFP_WANTED, NON_RFP_WANTED, DUMMY_COUNT_RFP_OPTION};
//enum check_morphology_options { NOTCHECKMORPHOLOGY, NORMAL_WANTED, DUMPY_WANTED, DUMMY_COUNT_MORPHO_OPTION};
//enum check_stage_options { NOTCHECKSTAGE, ADULT_WANTED, L4_WANTED, L3_WANTED, L2_WANTED, L1_WANTED, DUMMY_COUNT_STAGE_OPTION};

class phenotyping_method
{
public:
	bool check_sex;
	bool check_GFP;
	bool check_RFP;
	bool check_morphology;
	bool check_stage;

	phenotyping_method(
		bool to_check_sex,
		bool to_check_GFP,
		bool to_check_RFP,
		bool to_check_morphology,
		bool to_check_stage);

	phenotyping_method();

	// Sometimes phenotyping_method can be inferred from a specific phenotype wanted, e.g. if user want GFP, then check_GFP must be true
	phenotyping_method(Phenotype& ph_wanted);

	// Setter
	void setWantedPhenotype(sex_types sex_type, GFP_types gfp_type, RFP_types rfp_type, morph_types morph_type, stage_types stage_type);
	void setWantedPhenotype(Phenotype& Ph);

	// Getter
	Phenotype getWantedPhenotype() const;
private:
	Phenotype phenotype_wanted; // Phenotype object that holding params expected by current phenotyping_method

};

enum imaging_mode { BRIGHTFIELD = 0, LASER_AUTOFOCUS = 1, BARCODE_READING = 2, GFP_IMAGING = 3, RFP_IMAGING = 4};

// TODO: delete the deprecated version of the following desired phenotyping options for the current plate

extern phenotyping_method phenotyping_method_being_used; // Holds the phenotyping method for the current plate
extern std::ofstream experiment_data;
extern cv::Rect* move_offset_region;
extern std::vector<std::string> MOVE_TO_SCREEN_PLATE_DEFAULT;

// Interperet and execute script action for current plate using available hardware
bool executeScriptAction(PlateTray& Tr, ErrorNotifier& Err, WormPick &pk, OxCnc &Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder& worms, int& kg, CameraFactory& camFact, PlateTracker& plateTracker, WormSocket* sock_ptr = nullptr);

// Specific actions
// High-level scripts
bool executeCross(WormPick &Pk, PlateTray &Tr, OxCnc &Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder& worms, CameraFactory& camFact, int& kg);
bool executeCross(WormPick &Pk, PlateTray &Tr, OxCnc &Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder& worms, CameraFactory& camFact, int& kg, PlateTracker& plateTracker);
bool executeSingleWorms(WormPick &Pk, PlateTray &Tr, OxCnc &Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder& worms, CameraFactory& camFact, int& kg);
bool executeScreenPlates(WormPick &Pk, PlateTray &Tr, OxCnc &Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder& worms, CameraFactory& camFact, int& kg);

// Mid-level scripts
bool executePickNWorms(WormPick &Pk, PlateTray& Tr, OxCnc &Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder& worms, CameraFactory& camFact, int& kg, bool verify_drop_over_whole_plate);
bool executePickNWorms(WormPick& Pk, PlateTray& Tr, OxCnc& Ox, OxCnc& Grbl2, StrictLock<OxCnc>& oxLock, StrictLock<OxCnc>& grbl2Lock, WormFinder& worms, CameraFactory& camFact, int& kg, bool verify_drop_over_whole_plate, PlateTracker& plateTracker);
bool executeScreenOnePlate(WormPick &Pk, PlateTray &Tr, OxCnc &Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder& worms, CameraFactory& camFact, int& kg);

// Low-level scripts
bool executeSpeedPick(WormPick &Pk, PlateTray& Tr, OxCnc &Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder &worms, CameraFactory& camFact, int* num_expected_worms = nullptr);
//bool executeCenterWorm(WormPick &Pk, PlateTray &Tr, OxCnc &Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder& worms, CameraFactory& camFact, int& kg, Phenotype& worm_returned);
bool executeCenterWorm(WormPick &Pk, PlateTray &Tr, OxCnc &Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder& worms, CameraFactory& camFact, int& kg, std::vector<Phenotype>& phenotypes_found, int& num_worms);
bool executeHeatPick(WormPick &Pk, PlateTray &Tr, OxCnc &Grbl2);
bool executePhenotypeFluo(WormPick &Pk, const PlateTray& Tr, OxCnc& Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder& worms, CameraFactory& camFact);
bool focusImage(WormPick& Pk, OxCnc& Ox, WormFinder& worms, bool is_low_mag, bool move_pick_slightly_away);
bool executeMoveToPlate(WormPick& Pk, PlateTray& Tr, OxCnc& Ox, OxCnc &Grbl2, StrictLock<OxCnc>& oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder& worms, CameraFactory& camFact);
bool executeMoveToPlate(WormPick& Pk, PlateTray& Tr, OxCnc& Ox, OxCnc &Grbl2, StrictLock<OxCnc>& oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder& worms, CameraFactory& camFact, PlateTracker& plateTracker);
bool executeMoveRel(const PlateTray& Tr, OxCnc& Ox, StrictLock<OxCnc> &oxLock);
//bool executeMoveAndLift(WormPick& Pk, const PlateTray& Tr, OxCnc& Ox, OxCnc& Grbl2, StrictLock<OxCnc>& oxLock, StrictLock<OxCnc>& grbl2Lock);
//bool executeLiftAndDropLids(WormPick& Pk, const PlateTray& Tr, OxCnc& Ox, OxCnc& Grbl2, StrictLock<OxCnc>& oxLock, StrictLock<OxCnc>& grbl2Lock, std::vector<std::tuple<MatIndex, MatIndex>> lift_and_drop_locs);
bool executePlateMapping(WormPick& Pk, const PlateTray& Tr, OxCnc& Ox, OxCnc& Grbl2, StrictLock<OxCnc>& oxLock, StrictLock<OxCnc>& grbl2Lock, WormFinder& worms, CameraFactory& camFact);
bool executeCalibrateOxCnc(WormPick &Pk, const PlateTray& Tr, OxCnc& Ox, StrictLock<OxCnc>& oxLock, WormFinder& worms, OxCnc& Grbl2, CameraFactory& camFact);
bool executeCalibratePick(PlateTray& Tr, WormPick &Pk, OxCnc& Ox, StrictLock<OxCnc>& oxLock, WormFinder& worms);
bool executeCalibrateCam(CameraFactory& camFact);
bool executeFindWorm(WormPick& Pk, PlateTray& Tr, OxCnc& Ox, OxCnc& Grbl2, StrictLock<OxCnc>& oxLock, StrictLock<OxCnc>& grbl2Lock, WormFinder &worms, CameraFactory& camFact, WormSocket* sock_ptr);
bool executePlatePosRel(PlateTray& Tr); /// Shift the position of the plate - FOR THIS RUN ONLY DOES NOT SAVE - so pick goes to different place
bool executeFocusImage(WormPick& Pk, const PlateTray& Tr, OxCnc& Ox, StrictLock<OxCnc>& oxLock, WormFinder& worms);
bool executeRandomDropOffset(OxCnc &Ox, double& x_offset, double& y_offset);
bool executePhenotype(WormPick &Pk, const PlateTray& Tr, OxCnc& Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder& worms, CameraFactory& camFact, int& num_worms, std::vector<Phenotype>& phenotypes_inferred);
bool executePhenotypeV2(WormPick &Pk, const PlateTray& Tr, OxCnc& Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder& worms, CameraFactory& camFact, int& num_worms, std::vector<Phenotype>& phenotypes_inferred, bool save_frame = false);
bool executeTestDyDOxCalib(WormPick &Pk, const PlateTray& Tr, OxCnc& Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder& worms, CameraFactory& camFact);
bool executeAutoFocus(OxCnc &Ox, WormFinder& worms, CameraFactory& camFact, PlateTray& Tr);



/*
	Helper functions for various script steps
*/

void ImagingMode(imaging_mode mode, OxCnc &Grbl2, CameraFactory& camFact);
std::vector<cv::Point3d> convertCoord(std::vector<cv::Point> FOV_posit, cv::Point center_coord, cv::Point3d stage_coord, double dydOx);
void cvtPlateCoord(std::vector<cv::Point>& coord, cv::Size FOV_size, MatIndex idx);
std::string getPlateIdKey(PlateTray& Tr);
template<class T> void InputValidArgs(T& arg_input, const std::vector<T>& possible_valid_args = {});
std::vector<Phenotype> getWantedPhenotypeFromInput(int num_source_plates, std::vector<int>* num_worms_from_sources = nullptr);
//bool setAndRunPhenotyping(WormPick &Pk, const PlateTray& Tr, OxCnc& Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder& worms, CameraFactory& camFact, int& num_worms);
bool centerTrackedWormInHighMag(OxCnc& Ox, WormFinder& worms, WormPick& Pk, const PlateTray& Tr, bool wait_if_lost = true, bool use_mask_rcnn = false);
bool performIntermediatePickingStep(WormPick& Pk, PlateTray& Tr, OxCnc& Ox, OxCnc &Grbl2, StrictLock<OxCnc>& oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder& worms, CameraFactory& camFact, int& kg, int& num_worms, MatIndex int_loc, std::vector<Phenotype>& worm_phenotypes_found);
std::vector<MatIndex> generatePlateLocations(std::set<int> rows_with_plates, PlateTray& Tr, WormPick& Pk, std::vector<MatIndex> excluded_locations = {}, LidHandler::plate_type plate_type = LidHandler::plate_type::IS_SOURCE);
void stimulateMovementWithBlueLight(WormFinder& worms, OxCnc &Grbl2, CameraFactory& camFact, int secs_to_stimulate = 120);
bool clarifyDumpyStatus(WormFinder& worms, bool initial_dumpy_result);


void cycleWormScript(PlateTray &Tr, OxCnc &Ox, OxCnc& Grbl2, StrictLock<OxCnc> &oxLock,
	StrictLock<OxCnc> &grbl2Lock, int* ptr_kg_or_rflag_Script_thread_isReady, WormPick& Pk,
	WormFinder& worms, CameraFactory& camFact);


// Structure used for finding the closest worm
struct compDist {

	cv::Point ref_pt;

	bool operator () (cv::Point pt1, cv::Point pt2) {
		return (ptDist(pt1, ref_pt) < ptDist(pt2, ref_pt));
	}

};

// Structure used for finding the closest worm
struct compWormDist {

	cv::Point ref_pt;

	bool operator () (Worm worm1, Worm worm2) {
		return (ptDist(worm1.getWormCentroidGlobal(), ref_pt) < ptDist(worm2.getWormCentroidGlobal(), ref_pt));
	}

};


#endif