/*
* Generate and read barcode labels that contain serial numbers of plates.
* The serial number of a plate is used to look up the following information
* about the plate:
* - Plate number (int, primary key)
* - Strain name (string)
* - Generation number (int)
* - Creation date of the record (string)
*
* Yuying Fan and Siming He
*/

#pragma once

#include <string>
#include<bitset>
#include <windows.h>

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc.hpp>
#include "opencv2/imgcodecs.hpp"
#include "opencv2/highgui.hpp"

#include "ImageFuncs.h"
#include "Database.h"

class PlateTracker {
private:
	cv::Mat barcodeImage;  // the camera barcode image
	int id;  // the id associated with the stored barcode image

public:
	std::string filename; // the filename of the associated experiment file
	PlateTracker(std::string filename);
	PlateTracker(std::string filename, cv::Mat barcodeImage);

	cv::Mat getBarcodeImage() { return barcodeImage; }
	int getID() { return id; }

	// Set the barcode image and auto-update id based on image barcode
	void setBarcodeImage(cv::Mat barcodeImage);

	/*
* Generate a barcode label image encoding the next available id. The
* label image contains a barcode and the encoded plate number written
* beneath. The description the user enters will be displayed on the right
* with a line to write additional notes on. The description is preferred
* to be 60 characters or shorter (space included), or the writing will
* likely be cut off.
*
* @param description The short description of the plate from user input;
* should be under 60 characters.
* @return A Mat image that is the generated barcode.
*/
	cv::Mat generateBarcode(std::string description);

	/*
	* Generates a printable template containing the specified labels
	* and saves the template pages to the designated folder under the
	* filenames "barcodes_print_page_{num}.png". Be cautious that a call
	* to this function may overwrite previously generated templates in the
	* same folder path.
	* The template is letter-size. Assumes that all labels are the same size.
	*
	* @param folderPath The path to the folder to save the template images to.
	* @param labels The label images to print.
	* @param copies The number of copies to print for each label.
	*/
	void generateLabelPrintsFromMat(std::string folderPath, std::vector<cv::Mat>& labels, int copies);

	/*
	* Generates a printable template of new barcodes, each with a unique id,
	* and saves the template pages to the designated folder under the
	* filenames "barcodes_print_page_{num}.png". Be cautious that a call
	* to this function may overwrite previously generated templates in the
	* same folder, so it's the best to call it on a newly created folder.
    * The barcodes generated range from startNum to (startNum + numBarcodes - 1).
    * The template generated is intended to be print on letter-size.
	*
	* @param folderPath The path to the folder to save the template images to.
	* @param numBarcodes The number of unique labels to generate.
	* @param description The short description of the plate from user input;
	* should be under 60 characters. This will appear on every barcode generated.
	*/
	void generateUniqueLabelPrints(std::string folderPath, int numBarcodes, std::string description = "");

	// Read the plate with the id of the current barcode from database; return INVALID_PLATE if id doesn't exist
	Plate readCurrentPlate();

	// Given info of a plate, write it to the database with the id of the current barcode.
	// The creation date should be in the format "YYYY/MM/DD"; if a date is not given, the
	// current date will be used. 
	// This method doesn't do anything if the current id is negative.
	void writeCurrentPlate(std::string strain, std::map<Phenotype, int> phenotypes, int genNum, std::string creationDate = "");
};

/*
* Get the center line to scan along the barcode from the unprocessed camera image.
* Meant to function as a helper method.
*
* @param img_cropped The cropped section containing the barcode from the original
* barcode image.
* @param debug If set to true, show debug images displaying the centerline.
* @return A vector of all points consisting of the centerline from left to right.
*/
std::vector<cv::Point> getCenterline(cv::Mat& img_cropped, bool debug = false);

/*
* Decode the barcode from a barcode image with brightness threshold.
*
* @param img_gray The camera image of the barcode.
* @param debug If set to true, print in stdout the decoded number and checksum
* in decimal and binary.
* @return The decoded plate number from the barcode.
*/
int decodeBarcode(cv::Mat& img_gray, bool debug = false);

/*
* Decode the barcode from a barcode image with the ostu method.
*
* @param img_gray The camera image of the barcode.
* @param debug If set to true, print in stdout the decoded number and checksum
* in decimal and binary.
* @return The decoded plate number from the barcode.
*/
int decodeBarcodeOtsu(cv::Mat& img_gray, bool debug = false);

/*
* Display the straightened version of the label image for user to read.
*
* @param img_cropped The cropped section containing the barcode from the original
* barcode image.
* @return A Mat image that is the straightened version of the barcode section.
*/
cv::Mat straightenBarcodeLabel(cv::Mat& img_cropped);
