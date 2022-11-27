/*
	Script.h
	Anthony Fouad
	Fang-Yen Lab
	Fall 2018

	Definition of a Script for use with the plate tray. It's simply a type definition for a vector of script steps.
*/

#pragma once
#ifndef SCRIPT_H
#define SCRIPT_H

#include <vector>
#include "PlateTrayScriptStep.h"

typedef std::vector<PlateTrayScriptStep> Script;

#endif

