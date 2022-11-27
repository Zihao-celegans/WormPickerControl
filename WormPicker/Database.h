/*
 * Database.h
 *
 * Contains functions for reading from and writing to the plate tracking
 * database.
 * 
 * The database is implemented as a CSV file primarily sorted by plate id and secondarliy sorted
 * by chronological order. Each experiment has its own file. 
 * Data format: id,strain,phenotypes,genNum,creationDate
 * e.g. 1,dumpy,HERM GFP NON_RFP NORMAL ADULT : 2 HERM NON_GFP NON_RFP NORMAL ADULT : 4,0,20220118
 * 
 * Yuying Fan
 * Fang-Yen Lab
 * Jan 2022
 */

#pragma once

#include <string>
#include <iostream>
#include <ostream>
#include <fstream>
#include <sstream>
#include <map>
#include <stdio.h>

#include "Worm.h"

// TODO: add notes field for user notes
struct Plate {
	int id;  // primary key
	std::string strain;
	std::map<Phenotype, int> phenotypes;  // dict of phenotype to number of worms (initially added or last counted)
	int genNum;  // generation number
	std::string creationDate;
	bool operator==(const Plate& other) const {
		return id == other.id && strain.compare(other.strain) == 0 && phenotypes == other.phenotypes
			&& genNum == other.genNum && creationDate.compare(other.creationDate) == 0;
	}
	bool operator!=(const Plate& other) const {
		return !(other == *this);
	}
};

const Plate INVALID_PLATE = { -1, "N/A", {}, -1, "N/A" };

/*
 * Write a plate's info to the database. Maintains the invariant that
 * all plate records are sorted primarily by plate id and secondarily
 * by chronological order.
 *
 * @param filename The name of the database file for the current
 * experiment.
 * @param p The plate to write to the database.
 */
void writePlate(std::string filename, Plate p);

/*
 * Return the info of the plate specified by the given id in the database.
 * 
 * @param filename The name of the database file for the current
 * experiment.
 * @param plateId The id of the plate of interest.
 * @return The plate info, or INVALID_PLATE if the plate is not in
 * the database.
 */
Plate readPlate(std::string filename, int plateId);

/*
* Retrieve the next available plate ID for this experiment.
* 
* @param filename The name of the database file for the current experiment.
* @return The next available plate ID for this experiment.
*/
int getNextAvailableID(std::string filename);

/*
* Update the next available plate ID for this experiment to the
* designated number.
* 
* @param filename The name of the database file for the current experiment.
* @param n The number that is the new next available plate ID.
*/
void updateNextAvailableID(std::string filename, int n);

// helper function to convert Plate.phenotypes to string
// format: {phenotype_traits:ratio}
std::string phenotypesToString(Plate p);
