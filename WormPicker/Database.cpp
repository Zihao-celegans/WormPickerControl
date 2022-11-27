/*
 * Database.cpp
 *
 * Implements functions for reading from and writing to the plate tracking
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

#include "Database.h"

using namespace std;

// helper function to check if a file exists
inline bool file_exists(const std::string& name) {
	struct stat buffer;
	return (stat(name.c_str(), &buffer) == 0);
}

// helper function to convert Plate.phenotypes to string
// format: {phenotype_traits:ratio}
string phenotypesToString(Plate p) {
	string out = "";
	for (auto const& x : p.phenotypes) {
		out += sexTypeStrings[x.first.SEX_type];
		out += " ";
		out += gfpTypeStrings[x.first.GFP_type];
		out += " ";
		out += rfpTypeStrings[x.first.RFP_type];
		out += " ";
		out += morphTypeStrings[x.first.MORPH_type];
		out += " ";
		out += stageTypeStrings[x.first.STAGE_type];
		out += " : " + to_string(x.second) + " ";
	}
	return out.substr(0, out.length() - 1);  // remove the extra space at the end
}

void writePlate(std::string filename, Plate p) {
	if (file_exists(filename)) {
		ifstream ExperimentFile(filename);
		char tempName[] = "db_temp.txt";
		ofstream tempFile(tempName); //Temporary file
		if (!ExperimentFile || !tempFile) {
			cout << "Error opening files when writing to DB" << endl;
			return;
		}
		string line, cell;
		int i = -1;
		// Copy the next available id line
		getline(ExperimentFile, line);
		tempFile << line << '\n';
		// Copy the header line
		getline(ExperimentFile, line);
		tempFile << line << '\n';
		// Copy the top part of the database until the insert position
		while (getline(ExperimentFile, line)) {
			stringstream ss(line);
			getline(ss, cell, ',');
			i = stoi(cell);
			if (i > p.id) {
				break;
			}
			tempFile << line << '\n';
		}
		// Insert the new plate line
		tempFile << p.id << ",";
		tempFile << p.strain << ",";
		tempFile << phenotypesToString(p) << ",";
		tempFile << p.genNum << ",";
		tempFile << p.creationDate << endl;
		// Copy the bottom part of the database after the insert position
		if (i > p.id) {
			tempFile << line << '\n';
			while (getline(ExperimentFile, line)) {
				tempFile << line << '\n';
			}
		}
		ExperimentFile.close();
		tempFile.close();
		// Delete the original file
		char* filename_char_arr;
		filename_char_arr = &filename[0];
		remove(filename_char_arr);
		// Rename the new file
		rename(tempName, filename_char_arr);
	}
	else {
		// Create a new file
		ofstream ExperimentFile;
		ExperimentFile.open(filename);
		ExperimentFile << "0" << endl;  // the next currently available id
		ExperimentFile << "Plate ID,Strain,Phenotypes,Generation Number,Date Created" << endl;  // the header row
		// Write plate info as a new line at the end of the file
		ExperimentFile << p.id << ",";
		ExperimentFile << p.strain << ",";
		ExperimentFile << phenotypesToString(p) << ",";
		ExperimentFile << p.genNum << ",";
		ExperimentFile << p.creationDate << endl;
		// Close the file
		ExperimentFile.close();
	}
}

Plate readPlate(std::string filename, int plateId) {
	Plate p = INVALID_PLATE;

	ifstream ExperimentFile(filename);
	if (!ExperimentFile.is_open()) {
		cout << "Error opening file when reading from DB" << endl;
		return p;
	}

  string line, cell, word;
	int i = -1;

	// Skip the next available id line
	getline(ExperimentFile, line);
	// Skip the header line
	getline(ExperimentFile, line);
  // Read all lines, take the info from the most recent matching line
  while (i <= plateId && getline(ExperimentFile, line)) {
		stringstream ss(line);
		getline(ss, cell, ',');
		i = stoi(cell);
		if (i == plateId) {
			// Parse id
			p.id = plateId;
			// Parse strain
			getline(ss, p.strain, ',');
			// Parse phenotypes
			getline(ss, cell, ',');
			stringstream cs(cell);
			while (getline(cs, word, ' ')) {
				Phenotype phe;
				phe.SEX_type = (sex_types) distance(sexTypeStrings, find(sexTypeStrings, *(&sexTypeStrings + 1), word));
				getline(cs, word, ' ');
				phe.GFP_type = (GFP_types)distance(gfpTypeStrings, find(gfpTypeStrings, *(&gfpTypeStrings + 1), word));
				getline(cs, word, ' ');
				phe.RFP_type = (RFP_types)distance(rfpTypeStrings, find(rfpTypeStrings, *(&rfpTypeStrings + 1), word));
				getline(cs, word, ' ');
				phe.MORPH_type = (morph_types)distance(morphTypeStrings, find(morphTypeStrings, *(&morphTypeStrings + 1), word));
				getline(cs, word, ' ');
				phe.STAGE_type = (stage_types)distance(stageTypeStrings, find(stageTypeStrings, *(&stageTypeStrings + 1), word));
				getline(cs, word, ' ');  // skip the semicolon
				getline(cs, word, ' ');
				p.phenotypes.insert({ phe, stof(word) });
			}
			// Parse genNum
			getline(ss, cell, ',');
			p.genNum = stoi(cell);
			// Parse creationDate
			getline(ss, p.creationDate, ',');
		}
  }

  ExperimentFile.close();

	return p;
}

int getNextAvailableID(string filename) {
	ifstream ExperimentFile(filename);
	if (ExperimentFile.is_open()) {
		string line;
		getline(ExperimentFile, line);
		ExperimentFile.close();
		int id = stoi(line);
		if (id < 1048576) {  // cap id at 2^20 to prevent overflow on barcode
			return stoi(line);
		}
		else {
			updateNextAvailableID(filename, 0);
			return 0;
		}
	}
	else {
		ExperimentFile.close();
		// If the file does not exist, create it
		ofstream ExperimentFile;
		ExperimentFile.open(filename);
		ExperimentFile << "0" << endl;  // the next currently available id
		ExperimentFile << "Plate ID,Strain,Phenotypes,Generation Number,Date Created" << endl;  // the header row
		ExperimentFile.close();
		return 0;
	}
}

void updateNextAvailableID(string filename, int n) {
	ifstream ExperimentFile(filename);
	char tempName[] = "db_update_id_temp.txt";
	ofstream tempFile(tempName); //Temporary file
	if (!ExperimentFile || !tempFile) {
		cout << "Error opening files when updating next available ID" << endl;
		return;
	}
	string line;
	// Replace the first line to the new available ID
	getline(ExperimentFile, line);
	tempFile << n << '\n';
	// Copy the rest of the file
	while (getline(ExperimentFile, line)) {
		tempFile << line << '\n';
	}
	ExperimentFile.close();
	tempFile.close();
	// Delete the original file
	char* filename_char_arr;
	filename_char_arr = &filename[0];
	remove(filename_char_arr);
	// Rename the new file
	rename(tempName, filename_char_arr);
}
