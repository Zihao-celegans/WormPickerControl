/*
	PlateTrayScriptStep.h
	Anthony Fouad
	Fang-Yen Lab
	Fall 2018
	
	Contains information about one script step for use with a PlateTray object.
*/

#pragma once
#ifndef PLATETRAYSCRIPTSTEP_H
#define PLATETRAYSCRIPTSTEP_H

#include "MatIndex.h"
#include <string>
#include <vector>

class PlateTrayScriptStep {

public:

	// Construct a blank script step
	PlateTrayScriptStep() {

		/// Set data as blank
		step_name = std::string("");
		step_action = std::string("");
		id = MatIndex(0, 0);
    args = {};

		/// Check valid names
		checkValidNames();
	}

	// Construct a script step from arguments
	PlateTrayScriptStep(std::string sn, int r, int c, std::string sa, std::vector<std::string> const args) {

		/// Fill in data
		step_name = sn;
		id = MatIndex(r, c);
		step_action = sa;
    this->args = args;

		/// Check valid names
		checkValidNames();
	}

	// Define valid names
	void checkValidNames() {

		/// Valid actions names
		valid_step_actions.push_back("WWEXP");

		/// Valid step names
		valid_step_names.push_back("action");
		valid_step_names.push_back("startloop");
		valid_step_names.push_back("endloop");

		/*
			TODO: validate that step_name and step_action are valid
		*/

	}

  int getNumScriptStepArgs() { return this->args.size(); }


	std::string step_name;		// Name of the step, such as "action" or "startloop" or "endloop"
	std::string step_action;	// Type of action to take on this step, such as "WWEXP
	MatIndex id;				// (row,col) coordinates of the plate to be used
	std::vector<std::string> args;	// Arguments for this step, such as number of loops and other details

protected:
	std::vector<std::string> valid_step_names;
	std::vector<std::string> valid_step_actions;
};

#endif