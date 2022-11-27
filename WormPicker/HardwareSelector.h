/*
	HardwareSelector.h
	Anthony Fouad
	Fang-Yen Lab
	February 2019

	Structure containing preferenes for which hardware components to use; loads from text file
*/
#pragma once
#ifndef HARDWARESELECTOR_H
#define HARDWARESELECTOR_H

#include <string> 
#include "Config.h"
#include <iostream>
#include "json.hpp"

class HardwareSelector {

public:

	// Constructor: Loads from text file
	HardwareSelector() {

		nlohmann::json config = Config::getInstance().getConfigTree("enabled components");

		vidfilepath = config["debug videos"].get<std::string>();
		img_max = config["ImgMax"].get<int>();
		cam = config["camera"].get<bool>();
		lj = config["labjack"].get<bool>();
		grbl = config["grbl"].get<bool>();
		dxl = config["dynamixel"].get<bool>();
		actuator = config["actuonix"].get<bool>();

	}

	// Public variables
	std::string vidfilepath;
	int img_max;
	bool cam, lj, grbl, dxl, actuator;
};

#endif // ! HARDWARESELECTOR_H
