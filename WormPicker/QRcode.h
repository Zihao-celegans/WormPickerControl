/*
	QRcode.h
	Anthony Fouad
	Fang-Yen Lab
	January 2019
	
	Decode QR codes containing row and column information using OpenCV's QR Reader

*/

#pragma once
#ifndef QRCODE_H
#define QRCODE_H

// Standard includes
#include <vector>
#include <string>

// OpenCV includes
#include "opencv2\opencv.hpp"

// Anthony's includes
#include "MatIndex.h"

class QRcode {

public:

	// Constructor 
	//QRcode();

	// Scan an image and store result
	bool scanAndDecode(cv::Mat imggray);

	// Convert the QR object bounding box to a vector of points
	std::vector<cv::Point> convertQrBbox(cv::Mat &bbox, int shift_x, int shift_y);

	// Extract row and column integers from the string
	MatIndex parseQrRowCol(std::string data);

	// Public variables

	bool location_determined; // QR code(s) were found, successfully interpereted, and if multiple, they match up
	MatIndex curr_qr_loc;		// Current location reported by one of more QR codess
	std::vector<MatIndex> all_qr_loc;
	std::vector<std::vector<cv::Point>> all_bounding_box;
	std::vector<std::string> all_decoded_data;
};


#endif