/*
* Generate and read barcode labels that contain serial numbers of plates.
* The serial number of a plate is used to look up the following information
* about the plate:
* - Plate number (int, primary key)
* - Strain name (string)
* - Generation number (int)
* - Creation date (string)
*
* Yuying Fan and Siming He
*/

#include <PlateTracker.h>
#include<fstream>
#include<iostream>
#include <sstream>
#include <ctime>

using namespace cv;
using namespace std;

// Hard-coded parameters
const int imageWidth = 2650;  // width of the entire label image (not including border)
const int imageHeight = 200;  // height of the entire label image (not including border)
const int borderWidth = 2;  // the width of the border
const int borderSpace = 0 + borderWidth;  // the space between the border and the edge of the label image (currently set to no space)
const int bitNumber = 20;  // the number of bits used to encode the plate number
const int thinWidth = 4;  // the width of a thin rectangle (0) in the barcode
const int thickWidth = 10;  // the width of a thick rectangle (1) in the barcode
const int fuzzinessOffset = 4;  // some extra margin for the white bars to account for printer fuzziness
const int upperLeftX = 400 + borderSpace;  // x-coordinate of upper left corner of the first barcode rectangle
const int upperY = 40 + borderSpace;  // y-coordinate of the top of the barcode rectangles
const int lowerY = 190 + borderSpace;  // y-coordinate of the top of the barcode rectangles
const int numMargin = 30;  // the space between barcode and number
const int textMargin = 250;  // the space between number and description
const int lineMargin = 250;  // the space between number and line
const int numHeight = (imageHeight / 2 + 10) + borderSpace;  // the height of the number
const int textHeight = 45 + borderSpace;  // the height of the description text
const int lineHeight = imageHeight - 20 + borderSpace;  // the height of the line

PlateTracker::PlateTracker(string filename) {
	this->filename = filename;
	barcodeImage = NULL;
	id = -1;
}

PlateTracker::PlateTracker(string filename, Mat img) {
	this->filename = filename;
	setBarcodeImage(img);
}

void PlateTracker::setBarcodeImage(Mat img) {
	barcodeImage = img;
	id = decodeBarcode(barcodeImage, false);
}

cv::Mat PlateTracker::generateBarcode(string description) {
	// Retrieve the next available plate id
	int plateNumber = getNextAvailableID(this->filename);
	// Update the next available plate id
	updateNextAvailableID(this->filename, plateNumber + 1);
	// Convert the number to binary
	bitset<bitNumber> plateNumberBinary = plateNumber;
	// Obtain checksum using modolo 8
	int checksum = plateNumber % 8;
	bitset<3> checkSumBinary = checksum;
	// Create a blank image with white background
	Mat image(imageHeight + 2 * borderSpace, imageWidth + 2 * borderSpace, CV_8UC1, Scalar(255));
	Point p1, p2;  // top left corner and bottom right corner
	int currentX = upperLeftX;
	// Draw rectangles for the plate number
	for (int i = 0; i < bitNumber; i++) {
		// draw the black rectangle for the current bit
		p1 = Point(currentX, upperY);
		if (plateNumberBinary[i]) {
			currentX += thickWidth;
		}
		else {
			currentX += thinWidth;
		}
		p2 = Point(currentX, lowerY);
		rectangle(image, p1, p2, Scalar(0), -1, LINE_8);
		// skip the opposite (thin/thick) amount of white space
		if (plateNumberBinary[i]) {
			currentX += thinWidth;
		}
		else {
			currentX += thickWidth;
		}
		// Skip some white space to account for printer fuzziness
		// (which makes each black bar thicker than it's supposed to be)
		currentX += fuzzinessOffset;
	}
	// Draw rectangles for the checksum
	for (int i = 0; i < checkSumBinary.size(); i++) {
		// draw the black rectangle for the current bit
		p1 = Point(currentX, upperY);
		if (checkSumBinary[i]) {
			currentX += thickWidth;
		}
		else {
			currentX += thinWidth;
		}
		p2 = Point(currentX, lowerY);
		rectangle(image, p1, p2, Scalar(0), -1, LINE_8);
		// skip the opposite (thin/thick) amount of white space
		if (plateNumberBinary[i]) {
			currentX += thinWidth;
		}
		else {
			currentX += thickWidth;
		}
		// Skip some white space to account for printer fuzziness
		// (which makes each black bar thicker than it's supposed to be)
		currentX += fuzzinessOffset;
	}
	// Draw a final thin rectangle to signal the end of the barcode
	p1 = Point(currentX, upperY);
	currentX += thinWidth;
	p2 = Point(currentX, lowerY);
	rectangle(image, p1, p2, Scalar(0), -1, LINE_8);
	// Write the plate number underneath the barcode
	string plateNumberString = to_string(plateNumber);
	currentX += numMargin;
	Point numUpperLeft(currentX, numHeight);
	putText(image, plateNumberString, numUpperLeft, FONT_HERSHEY_COMPLEX_SMALL, 2, Scalar(0), 3, LINE_AA);
	// Write the typed description
	Point textUpperLeft(currentX + textMargin, textHeight);
	putText(image, description, textUpperLeft, FONT_HERSHEY_COMPLEX_SMALL, 2, Scalar(0), 3, LINE_AA);
	// Draw a line for the user to write additional notes on
	Point lineStart(currentX + lineMargin, lineHeight);
	Point lineEnd(imageWidth - 60 - borderSpace, lineHeight);
	line(image, lineStart, lineEnd, Scalar(0), 3, LINE_8);
	// Draw the border
	Point borderUpperLeft(borderSpace - borderWidth / 2, borderSpace - borderWidth / 2);
	Point borderUpperRight(imageWidth + borderSpace + borderWidth / 2, borderSpace - borderWidth / 2);
	Point borderLowerLeft(borderSpace - borderWidth / 2, imageHeight + borderSpace + borderWidth / 2);
	Point borderLowerRight(imageWidth + borderSpace + borderWidth / 2, imageHeight + borderSpace + borderWidth / 2);
	line(image, borderUpperLeft, borderUpperRight, Scalar(0), borderWidth, LINE_8);
	line(image, borderUpperLeft, borderLowerLeft, Scalar(0), borderWidth, LINE_8);
	line(image, borderUpperRight, borderLowerRight, Scalar(0), borderWidth, LINE_8);
	line(image, borderLowerLeft, borderLowerRight, Scalar(0), borderWidth, LINE_8);
	// Return the label image
	return image;
}

void PlateTracker::generateLabelPrintsFromMat(string folderPath, vector<cv::Mat>& labels, int copies) {
	// Set the position and margin parameters
	const int verticalMargin = 150;  // margin space for the top and bottom of the page
	const int sideMargin = 100;  // margin space for the left and right sides of the page
	const int labelMargin = 2;  // vertical space between two adjacent labels
	const int centerMargin = 0; // space from label to centerline of the page (horizontal space between two adjacent labels = 2 * centerMargin)
	// Create a blank image with white background (letter size: 215.9 x 279.4 mm)
	const int letterSizeWidth = (imageWidth + 2 * borderSpace + sideMargin) * 2;
	const int letterSizeHeight = letterSizeWidth * 279.4 / 215.9;
	Mat image(letterSizeHeight, letterSizeWidth, CV_8UC1, Scalar(255));
	// Set coordinates for label images
	const int leftX = sideMargin;  // left X of a label on the left
	const int rightX = letterSizeWidth / 2 + centerMargin;  // left X of a label on the right
	// Put the labels on and save the template page(s)
	int pageNum = 1;
	int upperLeftX = leftX;
	int upperLeftY = verticalMargin;
	int pageSize = (letterSizeHeight - verticalMargin * 2) / (imageHeight + borderSpace * 2 + labelMargin) * 2;  // number of labels per page
	for (int i = 0; i < labels.size() * copies; i++) {
		// A full page has been populated; save and start the next page
		if (i != 0 && i % pageSize == 0) {
			if (CreateDirectory(folderPath.c_str(), NULL) || ERROR_ALREADY_EXISTS == GetLastError()) {
				imwrite(folderPath + "barcodes_print_page_" + to_string(pageNum) + ".png", image);
			}
			image = Scalar(255);
			upperLeftX = leftX;
			upperLeftY = verticalMargin;
			pageNum++;
		}
		// Populate the current barcode label on the page
		cv::Rect roi(upperLeftX, upperLeftY, labels[i / copies].cols, labels[i / copies].rows);
		labels[i / copies].copyTo(image(roi));
		// Update the upper left corner coordinates for the next label
		if (i % 2 == 0) {
			upperLeftX = rightX;
		}
		else {
			upperLeftX = leftX;
			upperLeftY += (imageHeight + 2 * borderSpace) + labelMargin;
		}
	}

	// Save the last page
	if (CreateDirectory(folderPath.c_str(), NULL) || ERROR_ALREADY_EXISTS == GetLastError()) {
		imwrite(folderPath + "barcodes_print_page_" + to_string(pageNum) + ".png", image);
	}
}

void PlateTracker::generateUniqueLabelPrints(std::string folderPath, int numBarcodes, std::string description) {
	vector<Mat> barcodes;
	for (int i = 0; i < numBarcodes; i++) {
		barcodes.push_back(generateBarcode(description));
	}
	generateLabelPrintsFromMat(folderPath, barcodes, 1);
}

Plate PlateTracker::readCurrentPlate() {
	return readPlate(this->filename, this->id);
}

void PlateTracker::writeCurrentPlate(string strain, map<Phenotype, int> phenotypes, int genNum, string creationDate) {
	if (this->id >= 0) {
        if (creationDate.empty()) {
            auto t = std::time(nullptr);
            auto tm = *std::localtime(&t);

            std::ostringstream oss;
            oss << std::put_time(&tm, "%Y/%m/%d");
            creationDate = oss.str();
        }
        writePlate(this->filename, { this->id, strain, phenotypes, genNum, creationDate });
    }
}

// Helper function to crop the barcode image to only the barcode section
Mat cropBarcode(Mat& img_gray) {
	return img_gray(Range(0, img_gray.rows / 4), Range(0, img_gray.cols));
}

/*
* Get the center line to scan along the barcode from the unprocessed camera image.
* Meant to function as a helper method.
*
* @param img_cropped The cropped section containing the barcode from the original
* barcode image.
* @param debug If set to true, show debug images displaying the centerline.
* @return A vector of all points consisting of the centerline from left to right.
*/
vector<Point> getCenterline(Mat& img_cropped, bool debug) {
	// Blurs the image to alleviate noise
	Mat img_blur;
	GaussianBlur(img_cropped, img_blur, Size(3, 3), 0);

	// Threshold the image into binary
	Mat img_binary;
	adaptiveThreshold(img_blur, img_binary, 255, ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY, 475, -10);

	// Filter out small white spots
	Mat filtered;
	filterObjects(img_binary, filtered, img_binary.rows * img_binary.cols / 80, INT_MAX);

	// Use the filtered binary image as a mask for label
	Mat masked;
	img_cropped.copyTo(masked, filtered);
	if (debug) {
		Mat masked_display;
		resize(masked, masked_display, Size(650, 650 * masked.rows / masked.cols));
		imshow("masked", masked_display);
		waitKey(2000);
	}

	// Divide the image up vertically into small strips and compute the centroid
	// of each region
	vector<Point> centroids;
	int numBins = 10;
	int divideWidth = masked.cols / numBins;
	for (int i = 0; i < masked.cols; i += divideWidth) {
		if (debug) {
			cout << "processing bin #" << i / divideWidth << endl;
		}
		Mat stripe = masked(Range(0, masked.rows), Range(i, min((i + divideWidth), masked.cols)));
		Moments m = moments(stripe, true);
		if (debug) {
			cout << "m.m10: " << m.m10 << endl;
			cout << "m.m00: " << m.m00 << endl;
			cout << "m.m01: " << m.m01 << endl;
		}
		if (m.m00 != 0 && m.m10 / m.m00 > 0 && m.m01 / m.m00 > 0) {
			Point centroid(m.m10 / m.m00, m.m01 / m.m00);
			centroids.push_back(Point(centroid.x + i, centroid.y));
		}
	}

	// Connect the centroids to generate the full centerline
	vector<Point> centerLine;
	if (centroids.size() > 0) {
		for (int i = 0; i < centroids.size() - 1; i++) {
			int X1 = centroids[i].x;
			int Y1 = centroids[i].y;
			int X2 = centroids[i + 1].x;
			int Y2 = centroids[i + 1].y;
			centerLine.push_back(centroids[i]);
			for (int x = X1 + 1; x < X2; x++) {
				centerLine.push_back(Point(x, Y1 + (x - X1) * (Y2 - Y1) / (X2 - X1)));
			}
		}
		centerLine.push_back(centroids[centroids.size() - 1]);
	}

	if (debug) {
		// Draw and show the centerline for debugging
		Mat img_centerline = img_cropped.clone();
		if (centerLine.size() > 0) {
			for (int i = 0; i < centerLine.size() - 1; i++) {
				line(img_centerline, centerLine[i], centerLine[i + 1], Scalar(0, 0, 255), 3, LINE_4);
			}
		}
		resize(img_centerline, img_centerline, Size(650, 650 * img_centerline.rows / img_centerline.cols));
		cout << "Length of final centerline: " << centerLine.size() << endl;
		imshow("centerline", img_centerline);
		waitKey(0);
	}
	
	return centerLine;
}

/*
 * Helper function for decode
 */
int getMean(Mat& img_cropped, Point centerLine, int sampling_height) {
	int mean = 0;
	int count = 0;
	for (int i = -sampling_height; i <= sampling_height; i++) {
		if (centerLine.y + i > 0 && centerLine.y + i < img_cropped.rows) {
			mean += (int)(img_cropped.at<uchar>(centerLine.y + i, centerLine.x));
			count++;
		}
	}
	return mean / count;
}

/*
* Decode the barcode from a barcode image.
*
* @param img_gray The camera image of the barcode.
* @param debug If set to true, print in stdout the decoded number and checksum
* in decimal and binary.
* @return The decoded plate number from the barcode.
*/
int decodeBarcode(Mat& img_gray, bool debug) {
	Mat img_cropped = cropBarcode(img_gray);

	int height = img_cropped.rows;
	int sampling_height = height / 30;

	Mat img_centerline = img_cropped.clone();

	cvtColor(img_centerline, img_centerline, CV_GRAY2RGB);

	// Get the center line
	vector<Point> centerLine = getCenterline(img_cropped, debug);
	vector<int> brightness;

	// Find the brightest and darkest ten percent of pixels
	for (int i = 0; i < centerLine.size(); i++) {
		brightness.push_back((int)(img_cropped.at<uchar>(centerLine[i].y, centerLine[i].x)));
	}
	sort(brightness.begin(), brightness.end());
	int sum1 = 0;
	int sum2 = 0;
	int num = (int) brightness.size() / 10;
	for (int i = 0; i < num; i++) {
		sum1 += brightness[i];
		sum2 += brightness[brightness.size() - 1 - i];
	}

	// calculate the threshold based on the brightest and darkest ten percent of pixels
	int threshold = 0;
	
	if (num != 0) {
		threshold = (int)(sum1 / num * 0.6 + sum2 / num * 0.4);
	}

	std::bitset<23> result;

	int idx = 0;

	int dark_len = 0;
	int bright_len = 0;

	// RGB color for visualization
	// Black: decoding not started
	// Green: decoding before green are discarded because the width of white region before the green is too long, i.e. not actual barcode
	// Blue: region that is recognized as black
	// Red: region that is recognized as white
	int r = 0;
	int g = 0;
	int b = 0;

	// Thresholding
	for (int i = 0; i < centerLine.size() - 1; i++) {
		int curr = getMean(img_cropped, centerLine[i], sampling_height);
		int nxt = getMean(img_cropped, centerLine[i+1], sampling_height);
		if (idx > 23) {
			break;
		}
		if (curr > threshold &&
			nxt <= threshold) {	// change from white region to black region
			r = 0;
			g = 255;
			b = 0;
			bright_len = i - bright_len;
			if (bright_len < 10) {	
				// if bright length is greater or equal to 10, it cannot be the correct barcode region
				idx++;
				// when we recognized a black and a white region, we compare their width to determine 0/1
				if (idx > 0 && idx <= 23) {
					if (dark_len > bright_len) {
						result.set(idx - 1, 1);
					}
					else {
						result.set(idx - 1, 0);
					}
				}
				r = 255;
				g = 0;
				b = 0;
			}
			else {
				idx = 0;
			}
			dark_len = i + 1;
		}
		else if (curr <= threshold &&
			nxt > threshold) {	// change from black region to white region 
			dark_len = i - dark_len;
			bright_len = i + 1;
			r = 0;
			g = 0;
			b = 255;
		}

		circle(img_centerline, Point(centerLine[i].x, centerLine[i].y + sampling_height), 1, Scalar(r, g, b), FILLED, LINE_8);
		circle(img_centerline, Point(centerLine[i].x, centerLine[i].y - sampling_height), 1, Scalar(r, g, b), FILLED, LINE_8);
	}

	std::bitset<3> checksumBinary;
	for (int i = 0; i < 3; i++) {
		checksumBinary.set(i, result[20 + i]);
	}

	std::bitset<20> number;
	for (int i = 0; i < 20; i++) {
		number.set(i, result[i]);
	}

	std::cout << "Binary decoded: " << result.to_string() << endl;

	int real_num = number.to_ulong();

	if (debug) {
		cv::imshow("centerline", img_centerline);
	}

	return real_num;
}

int decodeBarcodeOtsu(Mat& img_gray, bool debug) {
	Mat img_cropped = cropBarcode(img_gray);

	int height = img_cropped.rows;
	int sampling_height = height / 20;

	Mat img_centerline = img_cropped.clone();

	cvtColor(img_centerline, img_centerline, CV_GRAY2RGB);

	// Get the center line
	vector<Point> centerLine = getCenterline(img_cropped, debug);
	double threshold = 0;
	// ofstream bfile;
	// bfile.open("boutputCropped127.txt");

	int length = (int)centerLine.size();

	int region[21][10000] = { 0 };


	for (int i = 0; i < centerLine.size(); i++) {
		int sum = 0;
		for (int j = -sampling_height; j <= sampling_height; j++) {
			if (centerLine[i].y + j > 0 && centerLine[i].y + j < img_cropped.rows) {
				region[j+ sampling_height][i] = (int)(img_cropped.at<uchar>(centerLine[i].y + j, centerLine[i].x));
			}
			sum += region[j + sampling_height][i];
			// bfile << (int)(img_cropped.at<uchar>(centerLine[i].y + j, centerLine[i].x));
			// bfile << "\n";
		}
		// std::cout << sum / 21 << endl;
		circle(img_centerline, Point(centerLine[i].x, centerLine[i].y + sampling_height), 1, Scalar(0, 255, 0), FILLED, LINE_8);
		circle(img_centerline, Point(centerLine[i].x, centerLine[i].y - sampling_height), 1, Scalar(0, 255, 0), FILLED, LINE_8);
	}

	cv::Mat cropped_reg = cv::Mat(21, static_cast<int>(centerLine.size()), CV_8U, &region);
	// cv::Rect myROI(0, 0, static_cast<int>(centerLine.size()) - 1, 20);
	// cv::Mat cropped_reg = reg(myROI);

	// cv::Mat cropped_reg = reg(Range(0, 20), Range(0, static_cast<int>(centerLine.size())-1));

	cv::imshow("cropped_reg", cropped_reg);

	threshold  = cv::threshold(cropped_reg, cropped_reg, 1, 255, CV_THRESH_BINARY | CV_THRESH_OTSU);

	// cv::imshow("centerline", cropped_reg);

	std::bitset<23> result;

	int idx = 0;

	int dark_len = 0;
	int bright_len = 0;

	std::cout << "Threshold: ";
	std::cout << threshold << endl;
	std::cout << centerLine.size() << endl;
	std::cout << sampling_height << endl;
	

	for (int i = 0; i < centerLine.size() - 1; i++) {
		int curr = getMean(img_cropped, centerLine[i], sampling_height);
		int nxt = getMean(img_cropped, centerLine[i + 1], sampling_height);
		// cout << i << endl;
		if (idx > 23) {
			break;
		}
		if (curr > threshold &&
			nxt <= threshold) {
			bright_len = i - bright_len;
			if (idx > 0) {
				// cout << bright_len << endl;
				// cout << dark_len << endl;
				if (dark_len > bright_len) {
					result.set(idx - 1, 1);
				}
				else {
					result.set(idx - 1, 0);
				}
			}
			dark_len = i + 1;
			idx++;
		}
		else if (curr <= threshold &&
			nxt > threshold) {
			dark_len = i - dark_len;
			bright_len = i + 1;
		}
		if (((int)(img_cropped.at<uchar>(centerLine[i].y, centerLine[i].x))) > threshold) {
			circle(img_centerline, centerLine[i], 1, Scalar(0, 0, 255), FILLED, LINE_8);
		}
		else {
			circle(img_centerline, centerLine[i], 1, Scalar(255, 0, 0), FILLED, LINE_8);
		}
	}



	std::bitset<3> checksumBinary;
	for (int i = 0; i < 3; i++) {
		checksumBinary.set(i, result[20 + i]);
	}

	std::bitset<20> number;
	for (int i = 0; i < 20; i++) {
		number.set(i, result[i]);
	}

	std::cout << result.to_string() << endl;
	std::cout << number.to_string() << endl;

	int real_num = number.to_ulong();

	cv::imshow("centerline", img_centerline);

	return real_num;

	// bfile.close();

	/*
	vector<int> b_data;
	for (int i = 0; i < centerLine.size; i++) {
		b_data.push_back(img_cropped.at<uchar>(centerLine[i].y, centerLine[i].x));
	}

	vector<int> thr_data;

	cv::threshold(b_data, thr_data, 0, 255, CV_THRESH_BINARY | CV_THRESH_OTSU);

	for (int i = 0; i < centerLine.size; i++) {
		cout << thr_data[i] << endl;
	}
	*/

}

/*
* Display the straightened version of the label image for user to read.
*
* @param img_cropped The cropped section containing the barcode from the original
* barcode image.
* @return A Mat image that is the straightened version of the barcode section.
*/
Mat straightenBarcodeLabel(Mat& img_cropped) {
	Mat shifted = Mat::zeros(img_cropped.size(), img_cropped.type());
	vector<Point> centerLine = getCenterline(img_cropped, 1);
	int verticalOffset = centerLine[centerLine.size() / 2].y * 3 / 5;
	for (int i = 0; i < centerLine.size(); i++) {
		int col = centerLine[i].x;
		int topRow = centerLine[i].y - verticalOffset;
		for (int row = topRow; row < img_cropped.rows; row++) {
			shifted.at<uchar>(row - topRow, col) = img_cropped.at<uchar>(row, col);
		}
	}
	return shifted;
}
