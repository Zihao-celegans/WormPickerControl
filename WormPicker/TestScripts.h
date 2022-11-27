/*
	Rules for test scripts:

	1. function name should start with initials like "JL_AssessHeadTail()"

	2. Use modules we have already for functionality that already exists, like segmentWorm for
		segmenting and measuing fluorescence; avoid creating many similar test functions


*/
#pragma once

#ifndef TESTSCRIPTS_H
#define TESTSCRIPTS_H

#include "WormPick.h"
#include "PlateTray.h"
#include "OxCnc.h"
#include "StrictLock.h"
#include "WormFinder.h"
#include "CameraFactory.h"
#include "PlateTracker.h"
#include "WormSocket.h"


/*
	John's test scripts
*/

// I (Peter) believe that all of the below test scripts were written by John. I moved them from ScriptActions when I merged John's branch into mine.
bool testCenterLarva(WormPick &Pk, PlateTray &Tr, OxCnc &Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder& worms, int& kg);
bool executeFluoImg(WormPick &Pk, const PlateTray& Tr, OxCnc& Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, CameraFactory& camFact);
bool executeCenterFluoWorm(WormPick &Pk, const PlateTray& Tr, OxCnc& Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder& worms, CameraFactory& camFact);
bool executeSortFluoWorms(WormPick &Pk, PlateTray &Tr, OxCnc &Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder& worms, CameraFactory& camFact, int& kg);
bool executeMeasureFluo(WormPick &Pk, const PlateTray& Tr, OxCnc& Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder& worms, CameraFactory& camFact);
bool executeMeasureHeadTail(WormPick &Pk, const PlateTray& Tr, OxCnc& Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder& worms);
bool executeMeasrPixDiff(WormPick& Pk, const PlateTray& Tr, OxCnc& Ox, OxCnc& Grbl2, StrictLock<OxCnc>& oxLock, StrictLock<OxCnc>& grbl2Lock, WormFinder& worms, CameraFactory& camFact);
bool executeCenterLarva(WormPick &Pk, PlateTray &Tr, OxCnc &Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder &worms, int& kg);
bool executeLowerPicktoTouch(WormPick &Pk, PlateTray &Tr, OxCnc &Ox, OxCnc &Grbl2, WormFinder &worms, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, CameraFactory& camFact);
bool executeMaskRCNN(WormFinder &worms);
bool executeMoveOnClick(WormPick &Pk, OxCnc &Ox, WormFinder& worms);
bool executePickOnClick(WormPick &Pk, PlateTray &Tr, OxCnc &Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc> &grbl2Lock, WormFinder& worms, CameraFactory& camFact, int& kg);
bool executePhtyFluoPlt(WormPick &Pk, PlateTray &Tr, OxCnc &Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc>& grbl2Lock, WormFinder& worms, CameraFactory& camFact);
bool executeTrayCalibration(PlateTray &Tr);
bool executeSleepAssay(WormPick& Pk, PlateTray& Tr, OxCnc& Ox, OxCnc& Grbl2, StrictLock<OxCnc>& oxLock, StrictLock<OxCnc>& grbl2Lock, WormFinder &worms, CameraFactory& camFact, WormSocket* sock_ptr);
bool executeMeasureImgLatency(const PlateTray &Tr, OxCnc &Grbl2, CameraFactory& camFact, WormFinder& worms);
bool executeGeneticMappingAnalysis(WormFinder& worms);
bool executeGenomicIntegFluoAnalysis(WormFinder& worms);
bool executeFindWorMotelWell(PlateTray &Tr);

/*
	Anthony's test scripts
*/

void testCentralizedSegmentation();


/*
	Peter's test scripts
*/
bool executeFindAndFollowWorms(WormPick &Pk, PlateTray &Tr, OxCnc &Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc>& grbl2Lock, WormFinder& worms, CameraFactory& camFact);
bool executeTestAutoLidHandler(WormPick &Pk, PlateTray &Tr, OxCnc &Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc>& grbl2Lock, WormFinder& worms, CameraFactory& camFact);
bool executeTestAutoFocus(WormPick &Pk, PlateTray &Tr, OxCnc &Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc>& grbl2Lock, WormFinder& worms, CameraFactory& camFact);
bool executeTestPhenotyping(WormPick &Pk, PlateTray &Tr, OxCnc &Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc>& grbl2Lock, WormFinder& worms, CameraFactory& camFact);
bool executeTestBlueLightStimulus(WormPick &Pk, PlateTray &Tr, OxCnc &Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc>& grbl2Lock, WormFinder& worms, CameraFactory& camFact);

/*
	Yuying's test scripts
*/
bool executeTestReadBarcode(WormPick &Pk, PlateTray &Tr, OxCnc &Ox, OxCnc &Grbl2, StrictLock<OxCnc> &oxLock, StrictLock<OxCnc>& grbl2Lock, WormFinder& worms, CameraFactory& camFact, PlateTracker& plateTracker);

#endif // !TESTSCRIPTS_H