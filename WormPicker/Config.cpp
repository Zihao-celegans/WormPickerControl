#include "Config.h"

#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include "boost/filesystem/operations.hpp"
#include "ControlledExit.h"


using json = nlohmann::json;
using namespace std;
using namespace filenames;
using namespace boost::filesystem;


void Config::loadFiles() {
	
	// iterate over the files in the config directory
	/*
	vector<string> files;
	directory_iterator end;
	for (directory_iterator dir_itr(pconfig); dir_itr != end; ++dir_itr) {
		string ext = dir_itr->path().filename().extension().generic_string();
		if (ext == ".json") {
			files.push_back(dir_itr->path().filename().string());
		}
	}*/
	
	// TODO - verify that the driectory contains the necessary json files

	try {
		ifstream stream(filenames::config_json);
		config = json::parse(stream);

	}
	catch (...) {
		controlledExit(false, "Error loading config files");
	}

	return;
}

json Config::getConfigTree(const string& name) {

	// Load config file automatically if it hasn't been done already
	if (config.empty()) {
		loadFiles();
	}

	// Verify that the requested tree exists and return it
	json tree = config[name];
	nlohmann::detail::value_t a = tree.type();
	controlledExit(a != nlohmann::detail::value_t::null , "Failed to load group '" + name + "' in config.json. Please check that this group exists in the file and restart WormWatcher.");
	return tree;
}


/*

replace the json node with a new tree

{
	...
	name:old
	...
}
		|
		V
{
	...
	name:tree
	...
}

*/

void Config::replaceTree(std::string name, json tree) {
	// erase the old node
	json newNode;
	newNode[name] = tree;
	// add the new node
	config.update(newNode);
}

void Config::writeConfig() {
	fstream configfile;
	configfile.open(filenames::config_json, fstream::out | fstream::trunc);
	configfile << setw(4) << config;
}

int Config::readVal(nlohmann::json tree, std::string name, int default_val) {

	int val = tree[name].get<int>();

	if (val == BAD_JSON_READ) {
		cout << "ERROR: Failed to read JSON Value: " << name << " - Default value: " << default_val << " used instead." << endl;
		val = default_val;
	}
	return val;

}


double Config::readVal(nlohmann::json tree, std::string name, double default_val) {

	double val = tree[name].get<double>();

	if (val == BAD_JSON_READ) {
		cout << "ERROR: Failed to read JSON Value: " << name << " - Default value: " << default_val << " used instead." << endl;
		val = default_val;
	}

	return val;

}

bool Config::readVal(nlohmann::json tree, std::string name, bool default_val) {

	// Read as an int to allow checking
	int val = tree[name].get<int>();

	if (val == BAD_JSON_READ) {
		cout << "ERROR: Failed to read JSON Value: " << name << " - Default value: " << default_val << " used instead." << endl;
		val = default_val;
	}

	return val;

}


string Config::readVal(nlohmann::json tree, std::string name, string default_val) {

	string val = tree[name].get<string>();

	if (val == "BAD_JSON_READ") {
		cout << "ERROR: Failed to read JSON Value: " << name << " Default value used instead." << endl;
		val = default_val;
	}

	return val;

}


