#include "WorMotel.h"

// Boost includes
#include <boost/filesystem.hpp>

// Zihao's includes
#include "Config.h"
#include "ImageFuncs.h"
#include "AnthonysColors.h"
#include "AnthonysCalculations.h"
#include "AnthonysTimer.h"


// Namespace
using namespace cv;
using namespace std;
using json = nlohmann::json;

WorMotel::WorMotel() {

	json WorMotel_config = Config::getInstance().getConfigTree("WorMotel");

	// path for raw image for getting template
	img_path_to_get_temp = WorMotel_config["MoatCrossTemplate"]["img_path"].get<string>();

	// get the template cropping params from config file
	p_x = WorMotel_config["MoatCrossTemplate"]["p_x"].get<double>();
	p_y = WorMotel_config["MoatCrossTemplate"]["p_y"].get<double>();
	p_width = WorMotel_config["MoatCrossTemplate"]["p_width"].get<double>();
	p_height = WorMotel_config["MoatCrossTemplate"]["p_height"].get<double>();

	// get the dsfactor
	dsfactor = WorMotel_config["dsfactor"].get<double>();

	// Get the threshold for template matching result
	temp_match_thresh = WorMotel_config["temp_match_thresh"].get<int>();
	
	// Get the intensity expand factor to calculate intensity range for intensity filtering
	inten_range_factor = WorMotel_config["inten_range_factor"].get<double>();

}


void WorMotel::getMoatCrossTemplate() {

	// p_x and p_y: coordinate of top left corner of ROI, in terms of percentage of width and height
	// p_width and p_height: width and height of the ROI, in terms of percentage of width and height
	// dsfactor: down sample factor

	cv::Mat raw_img = imread(img_path_to_get_temp);

	cv::cvtColor(raw_img, raw_img, cv::COLOR_BGR2GRAY);

	// Down sample
	resize(raw_img, raw_img, Size(), dsfactor, dsfactor);

	cv::imshow("Raw Image", raw_img);
	waitKey(500);

	cv::Rect ROI(raw_img.cols * p_x, raw_img.rows * p_y, raw_img.cols * p_width, raw_img.rows * p_height);
	CrossTemplate = raw_img(ROI);

	cv::imshow("Template", CrossTemplate);
	waitKey(500);

	return void();
}



// codes adapted from https://docs.opencv.org/3.4/de/da9/tutorial_template_matching.html
void WorMotel::DetectMoatCrossCoord(std::string img_path, std::string save_path, cv::TemplateMatchModes MatchMode, bool show_debug) {
	// Extracting path of individual image stored in a given directory
	std::vector<cv::String> WMImgs;
	// Path of the folder containing checkerboard images
	cv::glob(img_path, WMImgs);

	// image variables
	cv::Mat color_img, gray, mask;

	// determine whether the method accept mask
	bool method_accepts_mask = (TM_SQDIFF == MatchMode || MatchMode == TM_CCORR_NORMED);


	for (int i{ 0 }; i < WMImgs.size(); i++) {


		std::cout << "Image " << i << " : " << WMImgs[i] << std::endl;

		// Get images
		color_img = cv::imread(WMImgs[i]);
		

		AnthonysTimer Ta;

		// resize frame for faster computation
		resize(color_img, color_img, Size(), dsfactor, dsfactor);
		cv::cvtColor(color_img, gray, cv::COLOR_BGR2GRAY);



		cv::Mat img_display;
		gray.copyTo(img_display);

		cv::Mat result;
		int result_cols = gray.cols - CrossTemplate.cols + 1;
		int result_rows = gray.rows - CrossTemplate.rows + 1;
		result.create(result_rows, result_cols, CV_32FC1);

		// Template matching
		if (method_accepts_mask)
		{
			matchTemplate(gray, CrossTemplate, result, MatchMode, mask);
		}
		else
		{
			matchTemplate(gray, CrossTemplate, result, MatchMode);
		}

		// Normalize the result
		normalize(result, result, 0, 255, NORM_MINMAX, -1, Mat());



		// save reponse to folder
		std::string test_img_p = WMImgs[i].operator std::string();
		boost::filesystem::path p(test_img_p);

		cout << p.filename().string() << endl;

		// Save undistorted images to folder
		imwrite(save_path + p.stem().string() + "_result.jpg", result);


		//  Threshold to obtain the spots with high coreelation
		Mat thresh_result;
		threshold(result, thresh_result, temp_match_thresh, 255, THRESH_BINARY_INV);

		// Save thresholded images to folder
		imwrite(save_path + p.stem().string() + "_thresh_result.jpg", thresh_result);


		// Show pattern matching result

		if (show_debug) {
			imshow("Image", img_display);
			waitKey(500);
			imshow("Result", result);
			waitKey(500);
			imshow("Threhsolded Result", thresh_result);
			waitKey(500);
		}

		thresh_result.convertTo(thresh_result, CV_8UC1);

		// Get the contours of thresholded result
		vector<vector<Point>> Moat_cross_contours;
		vector<Vec4i> hierarchy;
		findContours(thresh_result, Moat_cross_contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

		// Get the centroid of each contour
		vector<Point> high_corr_spots; // coordinates of moat cross having high coorelation with the template
		centroidOfContours(Moat_cross_contours, high_corr_spots);


		// Exact high coorelated spot
		vector<Point> moat_cross_coords;
		PointsOffset(high_corr_spots, moat_cross_coords, CrossTemplate.cols * 0.5, CrossTemplate.rows * 0.5);


		// Draw the cross for the centroids
		Mat centroid_debug; thresh_result.copyTo(centroid_debug);
		cvtColor(centroid_debug, centroid_debug, COLOR_GRAY2RGB);
	

		// Extrapolate the grid of centroids
		vector<Point> extrap_Grid_pts;
		TwoDimGridExtrapolate(moat_cross_coords, extrap_Grid_pts, gray.size(), 0.05);

		Mat debug_grid; color_img.copyTo(debug_grid);
		drawCrossesForPoints(debug_grid, moat_cross_coords, colors::gg, 2, 4);
		drawCrossesForPoints(debug_grid, extrap_Grid_pts, colors::rr, 1, 2);

		imwrite(save_path + p.stem().string() + "_overlay_result.jpg", debug_grid);

		if (show_debug) {
			imshow("Extrap_centroids", debug_grid);
			waitKey(500);
		}

		// Get the intensity of each region surrounding high coorelated spots
		vector<Rect> vec_high_coor_Rois;
		getROIsfromPts(moat_cross_coords, gray.size(), vec_high_coor_Rois, 20);

		vector<double> inten_high_coor_spots;

		for (int i = 0; i < size(vec_high_coor_Rois); ++i) {
			double mean_inten = mean(gray(vec_high_coor_Rois[i]))[0];

			inten_high_coor_spots.push_back(mean_inten);
		}

		// Get the intensity range for those high coorelated region
		max_inten_moat_cross = *max_element(inten_high_coor_spots.begin(), inten_high_coor_spots.end());
		min_inten_moat_cross = *min_element(inten_high_coor_spots.begin(), inten_high_coor_spots.end());
		mean_inten_moat_cross = vectorMean(inten_high_coor_spots);
		std_inten_moat_cross = vectorStd(inten_high_coor_spots);


		cout << "min_inten_moat_cross = " << min_inten_moat_cross << endl;
		cout << "max_inten_moat_cross = " << max_inten_moat_cross << endl;
		cout << "mean_inten_moat_cross = " << mean_inten_moat_cross << endl;
		cout << "std_inten_moat_cross = " << std_inten_moat_cross << endl;

		
		// Get the upper and lower intensity threshold
		low_inten_thresh = mean_inten_moat_cross - inten_range_factor * std_inten_moat_cross;
		high_inten_thresh = mean_inten_moat_cross + inten_range_factor * std_inten_moat_cross;

		cout << "low_inten_thresh = " << low_inten_thresh << endl;

		cout << "high_inten_thresh = " << high_inten_thresh << endl;

		// Get average intensity of the region (size same as template) surrounding each grid point
		vector<Rect> vec_Rois;
		getROIsfromPts(extrap_Grid_pts, gray.size(), vec_Rois, 20);


		cout << "Mean intensity of template = " << mean(CrossTemplate) << endl;
		vector<double> intensity_grid_pt;
		vector<int> idx_grid_pt_to_keep;
		
		// Filter out the points with its too bright / dark surrounding
		for (int i = 0; i < size(vec_Rois); ++i) {
			double mean_inten = mean(gray(vec_Rois[i]))[0];
			//cout << "Mean intensity of surrounding grid point " << i << " = "<< mean_inten << endl;

			// if intensity fall in the range than record the index
			if (mean_inten >= low_inten_thresh && mean_inten <= high_inten_thresh) {
				idx_grid_pt_to_keep.push_back(i);
			}
		}

		// Get the points after intensity filtering
		vector<Point> grid_pts_to_keep;
		for (int i = 0; i < size(idx_grid_pt_to_keep); ++i) {
			grid_pts_to_keep.push_back(extrap_Grid_pts[idx_grid_pt_to_keep[i]]);
		}

		// show result
		Mat debug_inten_filt;  color_img.copyTo(debug_inten_filt);
		drawCrossesForPoints(debug_inten_filt, grid_pts_to_keep, colors::rr, 4, 4);

		imwrite(save_path + p.stem().string() + "_after_inten_filter.jpg", debug_inten_filt);

		if (show_debug) {
			imshow("After intensity filter", debug_inten_filt);
			waitKey(500);
		}

		cout << "Time for processing single image = " << Ta.getElapsedTime() << " s" << endl;
		// TODO: Perform AND operation of high_coord_spots and extra_grid
		//		intensity adaptive thresholding
	}

}
