/*

	Config.h
	Alexander Kassouni
	Fang-Yen Lab
	Summer 2019

	A singleton class for reading/writing to the config file

*/


#pragma once

#include <string>

#include "json.hpp"
#include "AnthonysCalculations.h"

namespace filenames {
	// Paths
	const std::string pparams = "C:\\WormPicker\\";
	const std::string plogs = "C:\\WormPicker\\logs\\";
	const std::string wmtemplates = "C:\\WormWatcher\\WorMotel templates\\";
	const std::string cnns = "C:\\WormPicker\\NeuralNets\\";

	// file names for plate tray parameters (where located and which ones to do)
	const std::string calibration_program = pparams + "WWPosInterp.exe";

	// script
	const std::string plate_script = pparams + "params_PlateTray_ActionScript.txt";

	// file name for debug events log and emailing
	const std::string debug_log = plogs + "debug_log_" + getCurrentDateString() + ".txt";
	const std::string email_program = pparams + "WWEmailNotify.exe";
	const std::string email_alert = pparams + "Email_Alert_Current.txt";


	// file name for WWAnalyzer
	const std::string wwa_program = pparams + "WWAnalyzer\\WWAnalyzer.exe";
	const std::string wwa_roi_files_dir = pparams + "WWAnalyzer\\ROI_Reference\\";

	// json
	const std::string config_json = pparams + "config.json";

	// Worm intensity profile
	const std::string worm_int_profile = pparams + "WormIntensityTemplate.txt";

	// Neural Networks
	const std::string cnn_picker_low_mag = cnns + "net_ds_0.4_97.23_worm.onnx";
	const std::string cnn_picker_hi_mag = cnns + "net_ds_0.25_94.561_wormPartsHiMag.onnx";
	//const std::string cnn_picker_gender = cnns + "net_3_81.348.onnx";
	const std::string cnn_picker_gender = cnns + "net_4_74.496_gender_beta.onnx";
}

// Singleton class for managing json config files
class Config
{
public:
	// only one config intance at any time
	static Config& getInstance() {
		static Config instance;
		return instance;
	}

	void loadFiles();
	
	nlohmann::json getConfigTree(const std::string& name);

	// write the json tree to the disk
	void writeConfig();
	
	void replaceTree(std::string name, nlohmann::json tree);

	// Load a variable with warning if it's not there
	int readVal(nlohmann::json tree, std::string name,int default_val);
	bool readVal(nlohmann::json tree, std::string name, bool default_val);
	double readVal(nlohmann::json tree, std::string name, double default_val);
	std::string readVal(nlohmann::json tree, std::string name, std::string default_val);

	
private:

	// maintain singleton design pattern
	Config() {}
	Config(Config const&) = delete;
	void operator=(Config const&) = delete;

	nlohmann::json config;	//the json tree containing all config parameters
};

