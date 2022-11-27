/*
	WormPicker.cpp
	Anthony Fouad
	Fang-Yen Lab
	Version 0.3 (February 2020) Adapted by Zihao Li
		- Equip Thorlabs fluorescence camera to acquire high-magnification bright field and fluorescence images
		- Display both low-mag and high-mag images in the main thread
	Version 0.2 (April 2018)
		- Second generation prototype based on OX CNC and DYNAMIXEL Servos.
		- Serial control interface uses AsyncSerial.h instead of SimpleSerial.h
			* AsyncSerial.h is based on multithreading. Stopping at a debug point interrupts ALL threads, including serial I/O.
			  If the debug point is hit before serial readout is completed, no readout will be printed to console, EVEN if the
			  debug point is after the read & print command in the main thread.
		- Notes about header file conflicts with OpenCV
			* "using namespace cv;" should not be in any header files. It causes a conflict between cv::ACCESS_MASK and
			  std::ACCESS_MASK when some windows libraries are included (perhaps buried in AsyncSerial.h).
			* all OpenCV objects like "Mat" must be referred to as cv::Mat in the header (.h) files.
			* HOWEVER, "using namespace cv;" can be safely inserted in cpp files to avoid using the cv:: prefix.
			* Even with this hygiene in use, there is an additional conflict with the file wingdi.h, which is apparently
			  included with windows.h (I actually have not figured out where windows.h is included because it does
			  not appear in my codes and I do not include stdafx.h). To avoid this conflict, I added "NOGDI" to
			  Project Properties -> Configuration Properties -> C/C++ -> Preprocessor -> Preprocessor Definitions
			  as described here: https://stackoverflow.com/questions/27064391/unwanted-header-file-wingdi-h
	Version 0.1 (February 2018)
		- First generation prototype using Leica stereoscope,
		movement of stage, and a single rotational servo
		
*/

#pragma once
// OpenCV includes
#include "opencv2\opencv.hpp"
#include "opencv2\dnn.hpp"


// Standard includes
#include <cstdio>
#include <iostream>
#include <deque>
#include <string>
#include <functional>
#include <direct.h>
#include <vector>

// Anthony's includes
#include "stdafx.h"
#include "AnthonysTimer.h"
#include "ThreadFuncs.h"
#include "AnthonysColors.h"
#include "ImageFuncs.h"
#include "WormFinder.h"
#include "WormPick.h"
#include "OxCnc.h"
#include "DynamixelController.h"
#include "PlateTray.h"
#include "FileNames.h"
#include "CameraFactory.h"
#include "StrictLock.h"
#include "ScriptActions.h"
#include "ErrorNotifier.h"
#include "HardwareSelector.h"
#include "Config.h"
#include "WPHelper.h"
#include "PlateMap.h"
#include "DnnHelper.h"
#include "TestScripts.h"
#include "PlateTracker.h"
#include "ImagingSourceCamera.h"

// Socket programming includes
#include "WormSocket.h"


// Boost includes 
#include "boost\asio.hpp"
#include "boost\thread.hpp"
#include <boost/algorithm/string.hpp>
#include <pylon/PylonIncludes.h>

// Test includes
#include <assert.h>


// Set namespaces
using namespace std;
using namespace cv;
using namespace std::chrono;
using json = nlohmann::json;


void main() {
	// NOTE: If you're running any worm segmentation in the high mag camera and using the python Mask RCNN network then you first must
	// start the server. You can do so by running the RCNN_Segment_Image_Launcher.bat file in the ActivityAnalyzer folder of the repo.


	/* ------------ Running test scripts before startup ---------------*/
	//testCentralizedSegmentation();
	//exit(0);


	/*--------------------- Hardcoded parameters ----------------------*/

	string pparams = "C:\\WormPicker\\";
	string plogs = "C:\\WormPicker\\logs\\";
	FileNames fname(pparams, plogs);
	Config::getInstance().loadFiles();

	/*--------------------- Startup sequence ----------------------*/

	Pylon::PylonAutoInitTerm autoInitTerm;

	// Create structure with information about which hardware components to use
	HardwareSelector use;

	// Create the debugging log, which is used to copy cout commands to a disk text file
	makePath(plogs);
	ofstream debug_log(filenames::debug_log, std::ofstream::app);

	// Error handler
	ErrorNotifier Err(debug_log);

	// Create worm pick object (includes DYNAMIXEL controller)
	bool in_pick_action = false;
	WormPick Pk(in_pick_action, use, Err); // Uncomment
	//WormPick Pk_tl(in_pick_action, use, Err);

	// Connect to the camera OR video file
	// CameraFactory handles lock creation
	CameraFactory& cameraFactory = CameraFactory::getInstance(use);
	cameraFactory.loadAllCameras();

	// Create shared file names (these are filled in to trigger saving in the appropriate thread) and paths (const for each cam)
	vector<shared_ptr<string>> full_save_name, save_path;
	for (int i = 0; i < cameraFactory.numberCams(); i++) {
		full_save_name.push_back(shared_ptr<string>(new string("")));
		save_path.push_back(shared_ptr<string>(new string("C:\\WormPicker")));
	}

	// Set up other shared variables
	string plate_name = "";
	bool request_image = true;
	bool dummy_mode = false;
	int k = -5, kg = -5;
	int display_cam = 0;
	int rflag_Script_thread_isReady = 0;
	int *ptr_kg_or_rflag_Script_thread_isReady = nullptr;

	// Load pick and worm CNN
	string fonnx_pick = "C:\\WormPicker\\NeuralNets\\net_ds0.2_99.36_PICK_TIP.onnx";
	//string fonnx_pick = "C:\\WormPicker\\NeuralNets\\net_4_98.438.onnx";
	string fonnx_worm = "C:\\WormPicker\\NeuralNets\\net_ds_0.4_97.23_worm.onnx";
	cv::dnn::Net net_pick = cv::dnn::readNetFromONNX(fonnx_pick);
	cv::dnn::Net net_worm = cv::dnn::readNetFromONNX(fonnx_worm);

	// Connect to OX CNC controllers
	//cout << "Debug exit" << endl; return void(); // Debug exit
	OxCnc Grbl2("Grbl2", Err, use, false, false); /// Small stepper controller
	OxCnc Ox("OxCnc", Err, use, true, true, &Grbl2);	/// CNC controller


	/// Create lock object for the GRBL objects
	StrictLock<OxCnc> oxLock(Ox), grbl2Lock(Grbl2);
	oxLock.unlock(); /// Constructor also locks it.
	grbl2Lock.unlock();

	// Create a PlateTray object containing info about the plates
	string current_plate_type;
	bool is_wormwatcher = false;
	PlateTray Tr(debug_log, Err, save_path, cameraFactory.getCamNums(), dummy_mode, current_plate_type, is_wormwatcher);
	Tr.resetPlateSeries(); // Picker only - always start the script at 0 for now. Build pausing/resuming later.


// Create a worm / blob object and declare loop variables
	Mat imgcolor, imggray, imgcolorwrite, imggraywrite, bw, background, imgfilt;
	Mat imgcolor_tl, imggray_tl, imgcolorwrite_tl, imggraywrite_tl, bw_tl, background_tl, imgfilt_tl;
	Mat imgcolor_bc, imggray_bc, imgcolorwrite_bc, imggraywrite_bc, bw_bc, background_bc, imgfilt_bc;
	char displaytext[300];
	char displaytext_tl[300];

	//string img_buffer = "C:\\WormPicker\\ImgBuffer\\";
	//string img_buffer_tl = "C:\\WormPicker\\ImgBuffer_tl\\";

	// Create low-mag (Main) camera
	shared_ptr<AbstractCamera> lowMagCam = cameraFactory.Nth_camera(0);
	shared_ptr<StrictLock<AbstractCamera>> lowMagCamLock = cameraFactory.getCameraLock(lowMagCam);
	lowMagCam->grabImage(*lowMagCamLock);
	// Set the exposure to the default value

	// Create Barcode (ImagingSource) camera
	shared_ptr<AbstractCamera> BarCodeCam = cameraFactory.Nth_camera(1);
	shared_ptr<StrictLock<AbstractCamera>> BarCodeCamLock = cameraFactory.getCameraLock(BarCodeCam);
	BarCodeCam->grabImage(*BarCodeCamLock);

	// Create high-mag (fluo) camera
	shared_ptr<AbstractCamera> fluoCam = cameraFactory.Nth_camera(2);
	shared_ptr<StrictLock<AbstractCamera>> fluoCamLock = cameraFactory.getCameraLock(fluoCam);
	fluoCam->grabImage(*fluoCamLock);

	// Cannot unlock twice, when the cam is created it is locked and normally the grabimage thread does the first unlock
	// SO if we use grabimage thread we don't use these unlocks
	//lowMagCamLock->unlock();
	//fluoCamLock->unlock();


	// Get the camera handle of fluorescence camera
	// To do: Rather than single camera handle, pass the whole camera object to script / plate cycling thread, a more organized way
	void* fluocam_handle = 0;
	fluoCam->getCamHandle(*fluoCamLock, fluocam_handle);
	//printf("Main thread: Fluo Camera handle = 0x%p\n", fluocam_handle);

	// Seed image variables
	lowMagCam->getCurrentImgColor(*lowMagCamLock).copyTo(imgcolor);
	lowMagCam->getCurrentImgGray(*lowMagCamLock).copyTo(imggray);
	imgcolor.copyTo(imgcolorwrite);
	imgcolor.copyTo(imggraywrite);

	fluoCam->getCurrentImgColor(*fluoCamLock).copyTo(imgcolor_tl);
	fluoCam->getCurrentImgGray(*fluoCamLock).copyTo(imggray_tl);
	imgcolor_tl.copyTo(imgcolorwrite_tl);
	imgcolor_tl.copyTo(imggraywrite_tl);


	BarCodeCam->getCurrentImgColor(*BarCodeCamLock).copyTo(imgcolor_bc);
	BarCodeCam->getCurrentImgGray(*BarCodeCamLock).copyTo(imggray_bc);
	imgcolor_bc.copyTo(imgcolorwrite_bc);
	imgcolor_bc.copyTo(imggraywrite_bc);

	// Create the worm / blob finders
	int watching_for_picked = NOT_WATCHING;
	Size sz_main_img = imggray.size();
	Size sz_fluo_img = imggray_tl.size();
	WormFinder WF(sz_main_img, watching_for_picked, fonnx_worm, imggray, imggray_tl, imgcolor_tl, imgcolor);
	//SortedBlobs worms_tl(imggray_tl.size(), watching_for_picked);
	vector<vector<Point>> contoursAll;
	bool show_bw = false;
	double focus_metric = 0;


	// Set up log file variables
	string current_LJ_error("");
	vector<shared_ptr<string>> current_cam_error;
	for (int i = 0; i < cameraFactory.numberCams(); i++)
		current_cam_error.push_back(shared_ptr<string>(new string("")));

	// Record frames y/n
	//VideoWriter outvid_raw; //VideoWriter outvid_labeled; // MainCam
	VideoWriter outvid_raw_tl; // FluorescenceCam
	VideoWriter outvid_color_and_tl;
	string fout_fragment, foutavi_raw, foutavi_labeled, fout_frames, fout_frames_tl, fouttxt;
	string fout_fragment_tl, foutavi_raw_tl, foutavi_color_and_tl;
	ofstream outtxt;
	bool recordflag = false; // To change to false
	bool saveframe = false;

	// Signal flag for maincam
	//unique_ptr<int> rflag1(new int(0));
	//unique_ptr<int> rflag2(new int(0));
	//int* rflag1 = new int(0);
	//int* rflag2 = new int(0);

	// Signal flag for fluocam
	shared_ptr<int> rflag_tl(new int(0));
	shared_ptr<int> rflag_color_and_tl(new int(0));
	//int* rflag_tl = new int(0);
	//int* rflag_color_and_tl = new int(0);

	// Create the folder hierarchy for storing fluo images in for later review if needed.
	//fluo_img_dir = "C:\\WormPicker\\Fluorescence_data\\Fluo_images\\";
	//_mkdir(fluo_img_dir.c_str());
	//fluo_img_dir.append("Fluo_images\\");
	//_mkdir(fluo_img_dir.c_str());

	//string fluo_dir_test_name = fout_fragment.size() > 0 ? fout_fragment : "non_recorded_test";
	//fluo_img_dir.append(getCurrentDateString()); fluo_img_dir.append("\\");  // fluo_img_dir.append("\\");

	//cout << "Fluo dir: " << fluo_img_dir << endl;
	//_mkdir(fluo_img_dir.c_str());
	//fluo_img_dir.append(fluo_dir_test_name);
	//_mkdir(fluo_img_dir.c_str());
	//fluo_img_dir.append("\\");

	// Create the folder hierarchy for storing phenotype images in for later review if needed.
	_mkdir(phenotype_img_dir.c_str());
	phenotype_img_dir.append(getCurrentDateString()); phenotype_img_dir.append("\\");
	_mkdir(phenotype_img_dir.c_str());
	string non_recorded_test_name("non_recorded_test_"); non_recorded_test_name.append(getCurrentTimeString(false, true));
	string fluo_dir_test_name = fout_fragment.size() > 0 ? fout_fragment : non_recorded_test_name;
	phenotype_img_dir.append(fluo_dir_test_name);
	_mkdir(phenotype_img_dir.c_str());
	phenotype_img_dir.append("\\");

	BF_img_dir = phenotype_img_dir; BF_img_dir.append("Brightfield\\"); _mkdir(BF_img_dir.c_str());
	GFP_img_dir = phenotype_img_dir; GFP_img_dir.append("GFP\\"); _mkdir(GFP_img_dir.c_str());
	RFP_img_dir = phenotype_img_dir; RFP_img_dir.append("RFP\\"); _mkdir(RFP_img_dir.c_str());


	// Set up threads in a vector
	vector<boost::thread> threads;

	/*for (shared_ptr<AbstractCamera> cam : cameraFactory.getCameras()) {
		auto threadfunc = boost::bind(&grabImageThreadNoFps, cam, cameraFactory.getCameraLock(cam), boost::ref(Err));
		threads.push_back(boost::thread(threadfunc));
	}*/

	/*auto fluoCam_threadfunc = boost::bind(&grabImageThreadNoFps, fluoCam, cameraFactory.getCameraLock(fluoCam), boost::ref(Err));
	threads.push_back(boost::thread(fluoCam_threadfunc));*/


	//auto barcodeCam_threadfunc = boost::bind(&grabImageThreadNoFps, barcodeCam, cameraFactory.getCameraLock(barcodeCam), boost::ref(Err));
	//threads.push_back(boost::thread(barcodeCam_threadfunc));

	/*auto lowMagCam_threadfunc = boost::bind(&grabImageThreadNoFps, lowMagCam, cameraFactory.getCameraLock(lowMagCam), boost::ref(Err));
	threads.push_back(boost::thread(lowMagCam_threadfunc));*/
	
	
	

	// Start up thread for displaying images
	bool wormSelectMode = false;
	string displaytextstr("");
	string displaytextstr_tl("");

	//threads.push_back(boost::thread(&displayImageThreadFunc,
	//									boost::ref(k),
	//									boost::ref(imgcolor),
	//									boost::ref(bw),
	//									boost::ref(Pk),
	//									boost::ref(worms),
	//									boost::ref(displaytextstr),
	//									boost::ref(wormSelectMode)));




	// Start up the labjack reading thread. Reads capacitive touch sensor as fast as possible.
	//if (use.lj)
	//	threads.push_back(boost::thread(&WormPick::readLabJackAIN0, &Pk, Ox.getLabJackHandle())); // uncomment

	// Start up the script / plate cycling thread.

	if (Tr.getListen_to_Socket() == false) {
		ptr_kg_or_rflag_Script_thread_isReady = &kg;
	}
	else if (Tr.getListen_to_Socket() == true) {
		ptr_kg_or_rflag_Script_thread_isReady = &rflag_Script_thread_isReady;
	}
	threads.push_back(boost::thread(&cycleWormScript,
		boost::ref(Tr),
		boost::ref(Ox),
		boost::ref(Grbl2),
		boost::ref(oxLock),
		boost::ref(grbl2Lock),
		ptr_kg_or_rflag_Script_thread_isReady,
		boost::ref(Pk),
		boost::ref(WF),
		boost::ref(cameraFactory)));

	// Start up thread for analyzing pick and worms

	//threads.push_back(boost::thread(&WormPick::segmentPickCnn, &Pk, boost::ref(imggray), boost::ref(net_pick), boost::ref(WF), true));
	threads.push_back(boost::thread(&WormFinder::runWormFinder, &WF));

	// Testing thread
	// threads.push_back(boost::thread(&WormPick::testCameraOffset, &Pk, &Grbl2, &worms));
	// Note: if you want to use testing you also need to switch the commenting in WormPick.cpp -> movePickByKey -> keyEND


	cout << "Record frames to disk?    FILE NAME  -or-  ENTER...\n\n";
	getline(cin, fout_fragment);


	//system("CLS");

	if (fout_fragment.size() > 0) {
		recordflag = true;
		global_exp_title = fout_fragment;
	}


	// Send the launch feedback and readiness of scripting thread back to socket
	// Adapted from: https://subscription.packtpub.com/book/application_development/9781783986545/1/ch01lvl1sec15/connecting-a-socket

	if (Tr.getListen_to_Socket() == true) {

		// Load parameters for set up socket and signal messages from config file
		json launch_socket_config = Config::getInstance().getConfigTree("API scripting")["program launch"];
		string raw_ip_address = launch_socket_config["raw_ip_address"].get<string>();
		unsigned short port_num = launch_socket_config["port_num"].get<unsigned short>();
		const string launch_success_signal = launch_socket_config["launch_success_signal"].get<string>();
		const string close_launch_socket_signal = launch_socket_config["close_launch_socket_signal"].get<string>();

		string response = "";

		Socket_Type s_type = CLIENT;
		WormSocket sock(s_type, raw_ip_address, port_num);

		// Step 5. Writing  data to the socket
		while (rflag_Script_thread_isReady != 1) {

			//cout << "rflag_Script_thread_isReady = " << rflag_Script_thread_isReady << endl;
			cout << "Waiting script execution thread to be ready..." << endl;
			boost_sleep_ms(1000);
		}

		// Wait a few seconds to make sure the script socket is ready for connection
		// TODO: operate socket in non-blocking mode so that the accept can be called before turning the ready flag to true
		boost_sleep_ms(2000);

		sock.send_data(launch_success_signal);
		cout << "	Launch feedback sent back to socket" << endl;

		while (!response.compare(close_launch_socket_signal)) {
			cout << "Waiting for signal for closing launch socket..." << endl;
			response = sock.receive_data();
			boost_sleep_ms(100);
		}

		sock.terminate();
		cout << "	Launch socket closed with success" << endl;
	}

	/////////////////////////////////////////////
	/*
	Up to this point, the robot is successful launched and ready to receive commands
	from either socket or user input
	*/
	/////////////////////////////////////////////



	if (recordflag) {

		// Ask for output file description

		// Get the date automatically
		char date[300];
		time_t t = time(NULL);
		struct tm buf;
		localtime_s(&buf, &t);

		if (buf.tm_mon + 1 < 10 && buf.tm_mday < 10) { sprintf_s(date, "%d-0%d-0%d", buf.tm_year + 1900, buf.tm_mon + 1, buf.tm_mday); }
		else if (buf.tm_mon + 1 >= 10 && buf.tm_mday < 10) { sprintf_s(date, "%d-%d-0%d", buf.tm_year + 1900, buf.tm_mon + 1, buf.tm_mday); }
		else if (buf.tm_mon + 1 < 10 && buf.tm_mday >= 10) { sprintf_s(date, "%d-0%d-%d", buf.tm_year + 1900, buf.tm_mon + 1, buf.tm_mday); }
		else if (buf.tm_mon + 1 >= 10 && buf.tm_mday >= 10) { sprintf_s(date, "%d-%d-%d", buf.tm_year + 1900, buf.tm_mon + 1, buf.tm_mday); }

		// Assemble final file name for video
		// Main camera record
		string recording_base_path;
		recording_base_path.append("C:\\WormPicker\\recordings\\"); recording_base_path.append(date); recording_base_path.append("_tests\\");
		_mkdir(recording_base_path.c_str()); // Create the folder in which to store today's recordings.
		recording_base_path.append(fout_fragment); recording_base_path.append("\\");
		_mkdir(recording_base_path.c_str()); // Create a folder for this specific test

		//Set the base directories for the videos and images
		foutavi_raw.append(recording_base_path);
		foutavi_labeled.append(recording_base_path);
		foutavi_raw_tl.append(recording_base_path);
		foutavi_color_and_tl.append(recording_base_path);
		fout_frames.append(recording_base_path);
		fout_frames_tl.append(recording_base_path);
		fouttxt.append(recording_base_path);

		foutavi_raw.append(date); foutavi_raw.append("_"); foutavi_raw.append(fout_fragment); foutavi_raw.append("_raw.avi");

		foutavi_labeled.append(date); foutavi_labeled.append("_"); foutavi_labeled.append(fout_fragment); foutavi_labeled.append("_labeled.avi");

		//foutavi_labeled.append("C:\\WormPicker\\recordings\\"); foutavi_labeled.append(date); foutavi_labeled.append("_tests\\"); foutavi_labeled.append(date); foutavi_labeled.append("_"); foutavi_labeled.append(fout_fragment); foutavi_labeled.append("_labeled.avi");
		// Fluo camera record
		foutavi_raw_tl.append(date);  foutavi_raw_tl.append("_"); foutavi_raw_tl.append(fout_fragment); foutavi_raw_tl.append("_high_mag.avi");
		foutavi_color_and_tl.append(date);  foutavi_color_and_tl.append("_"); foutavi_color_and_tl.append(fout_fragment); foutavi_color_and_tl.append("_color_and_tl.avi");

		//foutavi_raw_tl.append("C:\\WormPicker\\recordings\\"); foutavi_raw_tl.append(date); foutavi_raw_tl.append("_tests\\"); foutavi_raw_tl.append(date);  foutavi_raw_tl.append("_"); foutavi_raw_tl.append(fout_fragment); foutavi_raw_tl.append("_high_mag.avi");
		//foutavi_color_and_tl.append("C:\\WormPicker\\recordings\\"); foutavi_color_and_tl.append(date); foutavi_color_and_tl.append("_tests\\"); foutavi_color_and_tl.append(date);  foutavi_color_and_tl.append("_"); foutavi_color_and_tl.append(fout_fragment); foutavi_color_and_tl.append("_color_and_tl.avi");


		// Assemble first portion of file name for frame saving
		//fout_frames.append("C:\\WormPicker\\recordings\\"); fout_frames.append(date); fout_frames.append("_tests\\"); fout_frames.append(date); fout_frames.append("_"); fout_frames.append(fout_fragment); fout_frames.append("_raw");
		cout << "record dir: " << fout_frames << endl;
		//_mkdir(fout_frames.c_str()); // create a directory to contain the frames
		//fout_frames.append("\\");
		//fout_frames_tl.append("C:\\WormPicker\\recordings\\"); fout_frames_tl.append(date); fout_frames_tl.append("_tests\\"); //fout_frames_tl.append(date); fout_frames_tl.append("_"); fout_frames_tl.append(fout_fragment); fout_frames_tl.append("_high_mag");
		fout_frames_tl.append("high_mag_frames\\");
		_mkdir(fout_frames_tl.c_str()); // create a directory to contain the frames
		//fout_frames_tl.append("\\");

		// Open video writing object
		//outvid_labeled = VideoWriter(foutavi_labeled, VideoWriter::fourcc('M', 'J', 'P', 'G'), 10, imgcolor.size(), true);
		//outvid_raw = VideoWriter(foutavi_raw, VideoWriter::fourcc('M', 'J', 'P', 'G'), 10, imgcolor.size(), true);
		outvid_raw_tl = VideoWriter(foutavi_raw_tl, VideoWriter::fourcc('M', 'J', 'P', 'G'), 15, imgcolor_tl.size(), true);

		// Before we fill in the videowriter for the stitched color with tl video we need to get the overall size of the resulting video
		Size color_s = imgcolor.size();
		Size tl_s = imgcolor_tl.size();
		Size total_s = Size(color_s.width + tl_s.width, max(color_s.height, tl_s.height));

		outvid_color_and_tl = VideoWriter(foutavi_color_and_tl, VideoWriter::fourcc('M', 'J', 'P', 'G'), 15, total_s, true);


		bool test = outvid_color_and_tl.isOpened();

		// Assemble final file name for video metadata
		fouttxt.append(date); fouttxt.append("_"); fouttxt.append(fout_fragment); fouttxt.append(".txt");
		outtxt = ofstream(fouttxt);
	}


	// Start up the recording thread.
	// These will record a frame to disk only when signalled, and only if recording is enabled
	// As of Version 1.0 recording is only supported from the main camera.
	if (recordflag) {
		//threads.push_back(boost::thread(&WriteFrameToDisk_color, outvid_labeled, &imgcolorwrite, rflag1));
		//threads.push_back(boost::thread(&WriteFrameToDisk_raw, outvid_raw, &imggraywrite, rflag2));
		threads.push_back(boost::thread(&WriteFrameToDisk_tl, outvid_raw_tl, &imggraywrite_tl, rflag_tl));
		threads.push_back(boost::thread(&WriteFrameToDisk_color_with_tl, outvid_color_and_tl, &imgcolorwrite, &imggraywrite_tl, rflag_color_and_tl));
	}


	// Keep track of framerate
	double fps = 0;
	double waittime = 0;
	int& framenum = WP_main_loop_framenum;
	double frametime = 0;
	//Mat watching_ref_img;
	//Mat last_diff_img;


	// Set the mouse callback for Figure 1
	namedWindow("Figure 2", WINDOW_AUTOSIZE);
	//setMouseCallback("Figure 2", CallBackFunc, (void*)&worms_tl);

	namedWindow("Figure 1", WINDOW_AUTOSIZE);
	//setMouseCallback("Figure 1", CallBackFunc, (void*)&WF);

	cout << "Fig 1 clickable to find worm." << endl;

		/*--------------------- Main activity loop ----------------------*/

	while (true) {

		//cout << "Start Main Loop" << endl;
		AnthonysTimer Ta, Tb, Tt;
		framenum++;

		Tt.setToZero();
		// wait for previous frame to be done writing
		// cout << "Main thread->before writing: rflag1 = " << rflag1 << "; rflag2 = " << rflag2 << endl;
		while (
			//*rflag1 == 1 ||
			//*rflag2 == 1 ||
			*rflag_tl == 1 ||
			*rflag_color_and_tl == 1)
		{
			boost_sleep_ms(1);
			//cout << "writing " << *rflag1 << " " << *rflag2 << " " << *rflag_tl << " " << *rflag_color_and_tl << endl;
			//continue;
		}
		// cout << "Main thread->after writing: rflag1 = " << rflag1 << "; rflag2 = " << rflag2 << endl;

		///Tt.printElapsedTime("\n\n-------------------------------------\nWaiting for frame writing");

		// Grab image from the camera thread and convert it to gray
		Tt.setToZero();
		//cap >> imgcolor;
		//cv::cvtColor(imggray, imgcolor, IMREAD_GRAYSCALE);
		//imgcolor.copyTo(imggray);
		//cout << "	Cam 1" << endl;

		////AnthonysTimer tTest;
		//lowMagCamLock->lock();
		////////cout << "		Cam 1 Locked" << endl;
		//lowMagCam->getCurrentImgColor(*lowMagCamLock).copyTo(imgcolor);
		////////cout << "		Copy imgcolor" << endl;
		//lowMagCam->getCurrentImgGray(*lowMagCamLock).copyTo(imggray);
		////////cout << "Check point lowMagCam get gray------------" << endl;
		////////cout << "		Copy imgray" << endl;
		//lowMagCamLock->unlock();
		////tTest.printElapsedTime("Main Cam",true);


		lowMagCam->grabImage(*lowMagCamLock);
		lowMagCam->getCurrentImgColor(*lowMagCamLock).copyTo(imgcolor);
		lowMagCam->getCurrentImgGray(*lowMagCamLock).copyTo(imggray);

		//BarCodeCamLock->lock();
		////cout << "		Cam 1 Locked" << endl;
		//BarCodeCam->getCurrentImgColor(*BarCodeCamLock).copyTo(imgcolor_bc);
		////cout << "		Copy imgcolor" << endl;
		//BarCodeCam->getCurrentImgGray(*BarCodeCamLock).copyTo(imggray_bc);
		////cout << "		Copy imgray" << endl;
		//BarCodeCamLock->unlock();
		//cout << "Check point BarCodeCam ------------" << endl;


		//imshow("Initial", ImageQue.at(framenum % 20));
		//waitKey(1);

		/*imshow("Replaced", ImageQue[framenum % 20]);
		waitKey(1);*/

		////cout << "	Cam 2" << endl;
		//fluoCamLock->lock();
		//////cout << "		Cam 2 Locked" << endl;
		//fluoCam->getCurrentImgColor(*fluoCamLock).copyTo(imgcolor_tl);
		//////cout << "		Copy imgcolor" << endl;
		//fluoCam->getCurrentImgGray(*fluoCamLock).copyTo(imggray_tl);
		//////cout << "		Copy imggray" << endl;
		//fluoCamLock->unlock();


		/*tTest.printElapsedTime("Second Cam", true);
		string str = lowMagCam->getTimeStamp(*lowMagCamLock);
		string str_tl = fluoCam->getTimeStamp(*fluoCamLock);*/


		fluoCam->grabImage(*fluoCamLock);
		fluoCam->getCurrentImgColor(*fluoCamLock).copyTo(imgcolor_tl);
		fluoCam->getCurrentImgGray(*fluoCamLock).copyTo(imggray_tl);


		// Segment the pick by CNN
		//Pk.segmentPickCnn(imggray, net_pick, WF, false);
		// Segment the pick by conventional methods
		//Pk.segmentLoop(imggray, WF, imgfilt);


		///Tt.printElapsedTime("Grabbing frame @ " + str);

		{ // Putting curly braces around so I can collapse section.
			/*
				The below functionality has been moved into the runWormFinder and ParalWormFinder functions inside of the WormFinder class.
			*/

			//// Normally: Subtract background, and clear the difference / ref images
			//if (watching_for_picked == NOT_WATCHING) {
			//	Tt.setToZero();
			//	GaussianBlur(imggray, background, Size(31, 31), 31, 31);


			//	cv::subtract(background, Scalar(70), background);

			//	/*imshow("background", background);
			//	waitKey(1);*/

			//	cv::subtract(imggray, background, imggray);
			//
			//	normalize(imggray, imggray, 0, 255, NORM_MINMAX);

			//	//GaussianBlur(imggray_tl, background_tl, Size(21, 21), 21, 21);
			//	//cv::subtract(background_tl, Scalar(100), background_tl);
			//	//cv::subtract(imggray_tl, background_tl, imggray_tl);

			//	// Clear difference ref images
			//	watching_ref_img = Mat::zeros(Size(5, 5), CV_8U);
			//	last_diff_img = Mat::zeros(Size(5, 5), CV_8U);
			//}



			//// Alternatively (e.g. when scanning for picked up worm): Create difference image
			//else if (watching_for_picked == START_WATCHING) {
			//	imggray.copyTo(watching_ref_img);
			//	GaussianBlur(watching_ref_img, watching_ref_img, Size(5, 5), 5, 5);
			//	watching_ref_img.convertTo(watching_ref_img, CV_8S);
			//	watching_for_picked = WATCHING;
			//}

			//// Simply watching, compare to ref image, runs on same iter as start watching to start with zeros image
			//if (watching_for_picked == WATCHING) {

			//	// Blur the gray image slightly to reduce the noise drift over time
			//	GaussianBlur(imggray, imggray, Size(5, 5), 5, 5);

			//	// First get a difference image compared to the reference image and blur it again
			//	Mat A;
			//	imggray.convertTo(A, CV_8S);
			//	imggray =  A - watching_ref_img;
			//	imggray = cv::abs(imggray);
			//	imggray.convertTo(imggray, CV_8U);
			//	GaussianBlur(imggray, imggray, Size(5, 5), 5, 5);

			//	// Second, get the MAXIMUM difference image over time
			//	if (last_diff_img.rows == imggray.rows)
			//		imggray = cv::max(imggray, imggray);

			//	// Save this difference image so we can look for maximum next iter
			//	imggray.copyTo(last_diff_img);
			//}
		}
		//cout << "	1" << endl;
		//// Start writing image to disk immediately
		if (recordflag) {

			// Write image metadata to disk
			writeMetaData(frametime, outtxt, WF, Ox, Grbl2, Pk);

			// Write image to disk (note, this might be the previous frame/data, to allow labeling)
			//*rflag1 = 1;
			//*rflag2 = 1;
			*rflag_tl = 1;
			*rflag_color_and_tl = 1;
		}

		//// Write frame to disk (folder) for every 20 frames if the saveframe flag is TRUE
		int save_frame_num = 10;
		if (saveframe && (framenum % save_frame_num == 0)) {

			vector<int> params{ CV_IMWRITE_PNG_COMPRESSION, 0 }; // Set the paramters for saving frame. E.g. compression level

			/*string fout_frames_final = fout_frames;
			fout_frames_final.append(to_string(framenum)); fout_frames_final.append(".png");
			imwrite(fout_frames_final, imggray, params);*/

			string fout_frames_final_tl = fout_frames_tl;
			fout_frames_final_tl.append(fout_fragment); fout_frames_final_tl.append("_"); fout_frames_final_tl.append(to_string(framenum / save_frame_num)); fout_frames_final_tl.append(".png");
			imwrite(fout_frames_final_tl, imggray_tl, params);
		}

		//cout << "	2" << endl;
		/*
			The below code was how the images were previously transferred into the WormFinder class for processing.
			Now the imggray mat is referenced inside WormFinder so we can grab images when we need them.
		*/
		///// copy variables over to the worms class for later processing
		//// Retrieve low-mag precedent frame and update worm class
		//string readf = img_buffer;
		//readf.append(to_string(framenum % len_buffer));
		//readf.append(".png");
		//imread(readf, IMREAD_GRAYSCALE).copyTo(WF.pre_frame_lo_mag);
		//imggray.copyTo(WF.imggray);
		//imwrite(readf, imggray, { CV_IMWRITE_PNG_COMPRESSION, 0 });

		//// Retrieve high-mag precedent frame and update worm class
		//string readf_tl = img_buffer_tl;
		//readf_tl.append(to_string(framenum % len_buffer_tl));
		//readf_tl.append(".png");
		//imread(readf_tl, IMREAD_GRAYSCALE).copyTo(WF.pre_frame_hi_mag);
		//imggray_tl.copyTo(WF.img_high_mag);
		//imwrite(readf_tl, imggray_tl, { CV_IMWRITE_PNG_COMPRESSION, 0 });


// Update worm segmentation parameters from disk every 10 frames or so
		Tt.setToZero();
		//if (framenum % 10 == 0) { WF.loadParameters(); }

		// Since processing of imggray is done outside of the Camera classes, we need to re-enforce the color (display) image resolution
		cv::cvtColor(imggray, imgcolor, COLOR_GRAY2RGB);
		cv::resize(imgcolor, imgcolor, lowMagCam->getColorSize());
		cv::cvtColor(imggray_tl, imgcolor_tl, COLOR_GRAY2RGB);
		cv::resize(imgcolor_tl, imgcolor_tl, fluoCam->getColorSize());

		sprintf_s(displaytext, "OX: [%0.0f, %0.0f, %0.0f] | fps: %0.0f | Pk: %0.1f - %0.1f",
			Ox.getX(), Ox.getY(), Ox.getZ(), fps,
			Pk.getPickGradient(true), Pk.getPickGradient(false)); // uncomment

		///Tt.printElapsedTime("Display image");
		displaytextstr = string(displaytext);
		displaytextstr_tl = string(displaytext_tl);
		//cout << "	3" << endl;
		//cout << "Displaying image in main loop." << endl;
		display_image(k, imgcolor, imgcolor_tl, bw, bw_tl, Pk, WF, displaytextstr, wormSelectMode); //uncomment
		//cout << "	4" << endl;
		// Prepare the frames to write to disk next iteration
		Tt.setToZero();
		if (recordflag) {
			imgcolor.copyTo(imgcolorwrite);
			imggray.copyTo(imggraywrite);
			cv::cvtColor(imggraywrite, imggraywrite, CV_GRAY2BGR);

			imgcolor_tl.copyTo(imgcolorwrite_tl);
			//normalize(imggray_tl, imggray_tl, 0, 255, NORM_MINMAX);
			imggray_tl.copyTo(imggraywrite_tl);

			cv::cvtColor(imggraywrite_tl, imggraywrite_tl, CV_GRAY2BGR);
		}

		//if (!OxCnc::move_secondary_manual) {
		//	Grbl2.synchronizeXAxes(Ox.getXgoal());
		//}
		//else {
		//	Grbl2.updateOffset(Ox.getX());
		//}


		// User requests
		//cout << "	5" << endl;
		// Find threshold
		if (k == 116) { optimize_threshold(imggray, WF, use, Err); }

		// Select whether to move Ox or Grbl2 manually using numpad keys
		if (k == 103) {
			OxCnc::move_secondary_manual = !OxCnc::move_secondary_manual;
			cout << "Undercarriage control switched to: " << OxCnc::move_secondary_manual << endl;
		}

		//// Move DYNAMIXEL manually using (Pg up / Pg Dwn / Home / End)
		Pk.movePickByKey(k, Grbl2); //uncomment

		//// Calibrate the stage
		//// TODO**************************************************************
		////Tr.calibrateByKey(k, Ox.getXYZ(), *camLocks[display_cam], *cameras[display_cam]);

		// Let user specify a 1 or a 2 for DNN training purposes, e.g. 1 is good and 0 is bad
		if (k == '!') {
			Pk.action_success_user = 1; //uncomment
			cout << "Pick success set TRUE by user" << endl;
		}
		if (k == ')') {
			Pk.action_success_user = 0; //uncomment
			cout << "Pick success set FALSE by user" << endl;
		}

		// Start/reset script actions manually by pressing s or e
		if (k == 's') {
			Tr.startPlateSeries();
		}		/// Start running (load current state from disk) by pressing s

		if (k == 'w' && !WF.worm_finding_mode == 1) {
			WF.worm_finding_mode = 1;
		}
		else if (k == 'w') {
			WF.worm_finding_mode = 0;
		}

		//cout << "Main thread " <<worms.worm_finding_mode << endl;
		// findWorminWholeImg
		//if (k == 'w' && !worms.findWorminWholeImg == 1) {
		//	worms.findWorminWholeImg = 1;
		//}
		//else if (k == 'w') {
		//	worms.findWorminWholeImg = 0;
		//}


		if (k == 'e') {
			Tr.resetPlateSeries();

			Pk.stopLowering(); // uncomment

		}		/// Reset back to step 0 by pressing e

		// Manual abort scripted pick actions by pressing s

		// Show/Hide action overlays by pressing h
		if (k == 'h' && WF.overlayflag == true) { WF.overlayflag = false; }
		else if (k == 'h' && WF.overlayflag == false) { WF.overlayflag = true; }

		// Toggle between BW and GRAY image
		if (k == 'b' && show_bw) { show_bw = false; }
		else if (k == 'b') { show_bw = true; }

		// Display current Grbl position
		if (k == 'q') {
			cout << "Current robot position: " << Ox.queryGrblPos() << endl;
		}
		// Display camera options dialog by pressing c
		//if (k == 'c') { cameras[display_cam]->displayCamOptions(*camLocks[display_cam]); }

		// Manually run lid-lifting sequence
		if (k == 'l') {
			Pk.lid_handler.liftOrDropWithFirstGrabber(Ox);
		}


		/// Store a copy seperately for inspection by the GRBL moving thread, unless a
		///	movement / action is  already queued (kg value already greater than 0)
		if (kg < 0) { kg = k; }

		//Select worm to generate parameters by pressing m

		if (k == 'm' && !wormSelectMode) {
			wormSelectMode = true;
		}
		else if (k == 'm') {
			wormSelectMode = false;
			WF.writeParameters();
			WF.loadParameters();
		}

		// Manually test the worm recognition after dropping (create a new thread for it)
		if (k == keyBackSlash) {
			boost::thread debug_thread(&WormFinder::wasWormDropped, &WF);
		}

		// Manually toggle pickup watching mode
		if (k == keyForwardSlash) {
			watching_for_picked++;
			if (watching_for_picked > 2)
				watching_for_picked = 0;
			cout << "Watching for picked set to: " << watching_for_picked << endl;
		}

		// Exit
		if (k == 27) { break; }
		k = -5;

		// Measure time elapsed and hold FPS constant at 20 fps
		//cout << "	6" << endl;
		double fpsmax = 20;
		double frametimemin = 1 / fpsmax;
		waittime = max(frametimemin - Ta.getElapsedTime(), 0.0);
		boost::this_thread::sleep(boost::posix_time::milliseconds(waittime * 1000));

		if (framenum % 5 == 0) { fps = 1 / Tb.getElapsedTime(); }
		frametime = frametime + Tb.getElapsedTime();
		///Tt.printElapsedTime("all other");
		//cout << "	End Main Loop" << endl;
	}
	// End of active main loop

	// Close down all threads
	for (int i = 0; i < threads.size(); ++i) {
		threads[i].interrupt();
		threads[i].join();
	}

	// Close video writer and output text file
	if (recordflag) { //outvid_raw.release();
		//outvid_labeled.release();
		//outvid_raw_tl.release();
		outvid_color_and_tl.release();
		outtxt.close();
	}

	// Move pick away prior to destructing Pk
	Pk.movePickAway(); //uncomment

	// Lower the lid prior to destructing OxCnc
	//Ox.lowerLid();

	// Wait for threads to exit before destructing objects
	boost_sleep_ms(700);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////


