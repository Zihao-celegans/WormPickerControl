/*
	WPHelper.cpp
	Anthony Fouad
	July 2019 

	Helper functions for wormpicker that were formerly located in ImageFuncs.cpp. ImageFuncs.cpp now has only functions
	shared between wormpicker and wormwatcher, not functions specific to wormpicker
*/



#include "ImageFuncs.h"
#include "AnthonysKeyCodes.h"
#include "ThreadFuncs.h"
#include "WormPick.h"
#include "WPHelper.h"

using namespace cv;
using namespace std;


void segment_image(int thresh, const Mat &imgin, Mat &bwout, vector<vector<Point>> &contoursout, HardwareSelector &use) {

	// Smoothen image
	Size blursize(3, 3);
	//GaussianBlur(imgin, imgin, blursize, 2, 2);

	// Threshold TODO - fix exception
	threshold(imgin, bwout, thresh, use.img_max, THRESH_BINARY_INV);

	// Remove very small objects prior to analysis
	Size removesmallsize(5, 5);
	Mat kernel = getStructuringElement(MORPH_ELLIPSE, removesmallsize);
	morphologyEx(bwout, bwout, MORPH_OPEN, kernel);

	// Label connected components using contours, an array of points
	vector<vector<Point>> contoursTemp;
	vector<Vec4i> hierarchy;
	findContours(bwout, contoursTemp, hierarchy, RETR_CCOMP, CHAIN_APPROX_SIMPLE);

	// Keep only the highest level contours (outer contours) with at least 15 points (min perimeter)
	contoursout.clear();

	for (int i = 0; i < contoursTemp.size(); i++) {
		if (hierarchy[i][3] == -1 && contoursTemp.size() > 15) {
			contoursout.push_back(contoursTemp[i]);
		}
	}

}

double otsu_8u_with_mask(const Mat src, const Mat &mask) {
	const int N = 256;
	int M = 0;
	int i, j, h[N] = { 0 };
	for (i = 0; i < src.rows; i++)
	{
		const uchar* psrc = src.ptr(i);
		const uchar* pmask = mask.ptr(i);
		for (j = 0; j < src.cols; j++)
		{
			if (pmask[j])
			{
				h[psrc[j]]++;
				++M;
			}
		}
	}

	double mu = 0, scale = 1. / (M);
	for (i = 0; i < N; i++)
		mu += i * (double)h[i];

	mu *= scale;
	double mu1 = 0, q1 = 0;
	double max_sigma = 0, max_val = 0;

	for (i = 0; i < N; i++)
	{
		double p_i, q2, mu2, sigma;

		p_i = h[i] * scale;
		mu1 *= q1;
		q1 += p_i;
		q2 = 1. - q1;

		if (std::min(q1, q2) < FLT_EPSILON || std::max(q1, q2) > 1. - FLT_EPSILON)
			continue;

		mu1 = (mu1 + i * p_i) / q1;
		mu2 = (mu - q1 * mu1) / q2;
		sigma = q1 * q2*(mu1 - mu2)*(mu1 - mu2);
		if (sigma > max_sigma)
		{
			max_sigma = sigma;
			max_val = i;
		}
	}

	return max_val;
}

double threshold_with_mask(const Mat& src, Mat& dst, double thresh, double maxval, int type, const Mat& mask) {

	if (mask.empty() || (mask.rows == src.rows && mask.cols == src.cols && countNonZero(mask) == src.rows * src.cols))
	{
		// If empty mask, or all-white mask, use cv::threshold
		//thresh = cv::threshold(src, dst, thresh, maxval, type);
		//cout << "empty mask, or all - white mask" << endl;
		src.copyTo(dst);
	}
	else
	{
		// Use mask
		bool use_otsu = (type & THRESH_OTSU) != 0;
		if (use_otsu)
		{
			// If OTSU, get thresh value on mask only
			thresh = otsu_8u_with_mask(src, mask);
			// Remove THRESH_OTSU from type
			type &= THRESH_MASK;
		}

		// Apply cv::threshold on all image
		thresh = cv::threshold(src, dst, thresh, maxval, type);

		// Copy original image on inverted mask
		src.copyTo(dst, ~mask);
	}
	return thresh;
}

void DNN_segment_worm(int thresh, const Mat &imgin, Mat &bwout, vector<vector<Point>> &contoursout, HardwareSelector &use, cv::dnn::Net net_worm) {

	// Variables for DNN
	Mat img_inter, imgout;
	double dsfactor = 0.4;
	int display_class_idx = 0;
	double conf_thresh = 5.5;
	int norm_type = 1;

	// Smoothen image
	Size blursize(3, 3);
	GaussianBlur(imgin, img_inter, blursize, 2, 2);

	findWormsInImage(img_inter, net_worm, dsfactor, imgout, false, 2, display_class_idx, conf_thresh, norm_type);

	//threshold_with_mask(imgin, img_thres, 100, 255, THRESH_BINARY, imgout);

	bitwise_or(imgin, ~imgout, bwout);


	GaussianBlur(bwout, bwout, blursize, 2, 2);
	//adaptiveThreshold(bwout, bwout, use.img_max, ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY_INV, 3, 10);
	threshold(bwout, bwout, thresh, use.img_max, THRESH_BINARY_INV);
	//imshow("threshold", bwout);
	//waitKey(1);
	

	// Remove very small objects prior to analysis
	//Size removesmallsize(5, 5);
	//Mat kernel = getStructuringElement(MORPH_ELLIPSE, removesmallsize);
	//morphologyEx(bwout, bwout, MORPH_OPEN, kernel);
	//imshow("remove small", bwout);
	//waitKey(1);

	// Label connected components using contours, an array of points
	vector<vector<Point>> contoursTemp;
	vector<Vec4i> hierarchy;
	findContours(bwout, contoursTemp, hierarchy, RETR_CCOMP, CHAIN_APPROX_SIMPLE);

	// Keep only the highest level contours (outer contours) with at least 15 points (min perimeter)
	contoursout.clear();

	for (int i = 0; i < contoursTemp.size(); i++) {
		if (hierarchy[i][3] == -1 && contoursTemp.size() > 15) {
			contoursout.push_back(contoursTemp[i]);
		}
	}

}

void display_image(int& k, Mat &imgcolorin, Mat &imgcolorin_tl, const Mat &bwin, const Mat &bwin_tl, const WormPick& Pk,
	const WormFinder& WF, const string &message, const bool &wormSelectMode) {

	//normalize(imgcolorin_tl, imgcolorin_tl, 0, 255, NORM_MINMAX);

	// Determine colors to use
	Scalar drawcolor, rectcolor, pickedsearchcolor;
	Scalar wormcolor = colors::yy;
	int drawthickness;

	if (wormSelectMode) {
		wormcolor = colors::pp;
		drawcolor = colors::yy;
		drawthickness = 2;
	}
	else if (WF.isWormOnPickup()) {
		drawcolor = colors::gg; drawthickness = 2;
	}
	else if (WF.isWormTracked()) {
		drawcolor = colors::bb; drawthickness = 1;
	}
	else {
		drawcolor = colors::rr; drawthickness = 1;
	}

	if (Pk.action_success) 
		pickedsearchcolor = colors::gg;
	else
		pickedsearchcolor = colors::rr;


	if (Pk.in_pick_action) { rectcolor = colors::rr; }
	else { rectcolor = colors::kk; }

	// Only draw overlays if requested
	if (WF.overlayflag) {

		// Draw text message on color image
		Size sz = imgcolorin.size();
		rectangle(imgcolorin, Rect(1, 1, sz.width, 30), rectcolor, -1);
		putText(imgcolorin, message, Point(10, 22), 1, 1, colors::yy, 1);

		Size full_img_size = WF.imggray.size();

		// All of our coordinates and sizes are in regard to the full resolution image acquired by the camera, but because the 
		// display image is not being displayed at the full resolution we must scale everything we draw on the display
		double h_multiplier = double(sz.height) / double(full_img_size.height);
		double w_multiplier = double(sz.width) / double(full_img_size.width);

		// Draw the final contours on the image
		// drawContours(imgcolorin, WF.getWormContours(), -1, wormcolor, 1, 8);

		//// Draw the circles of WF according to centroids generated by CNN
		//if (!WF.all_worms.empty()) {
		//	for (int i = 0; i < WF.all_worms.size(); i++) {
		//		drawCross(imgcolorin, WF.all_worms[i].getWormCentroidGlobal(), colors::rr, 1, 8); // PeteQ: I'm assuming the coordinates that need to be passed here are the global coordinates?
		//	}
		//}

		/*if (!WF.HighEnergyCentroids.empty()) {
			for (int i = 0; i < WF.HighEnergyCentroids.size(); i++) {
				circle(imgcolorin, WF.HighEnergyCentroids[i], 5, colors::bb, 5);
			}
		}*/

		if (!WF.Monitored_ROIs.empty()) {
			for (int i = 0; i < WF.Monitored_ROIs.size(); i++) {
				rectangle(imgcolorin, WF.Monitored_ROIs[i], colors::rr, 2);
			}
		}

		if (WF.laser_focus_display) {
			//Size low_mag_size = worms.imggray.size();
			int width = full_img_size.width * WF.laser_focus_roi_w_mult;
			int height = full_img_size.height * WF.laser_focus_roi_h_mult;
			Rect low_mag_laser_rect = Rect((WF.high_mag_fov_center_loc.x - width / 2) * w_multiplier, (WF.high_mag_fov_center_loc.y - height / 2) * h_multiplier, width * w_multiplier, height * h_multiplier);
			rectangle(imgcolorin, low_mag_laser_rect, colors::yy, 2);
		}

		// Pick-up ROI
		//rectangle(imgcolorin, WF.pickup_Roi, colors::bb, 1);

		// Unloading ROI.
		/*Rect unload_roi = WF.unload_search_Roi;
		if (unload_roi.x != -1 && unload_roi.y != -1 && unload_roi.width != 0 && unload_roi.height != 0) {
			rectangle(imgcolorin, unload_roi, colors::bb, 0.2);
		}*/

		vector<Point> crawl_from = WF.origin_traj;
		for (int i = 0; i < crawl_from.size(); i++) {
			drawCross(imgcolorin, crawl_from[i], colors::gg, 2, 8);
		}

		// Calibration region
		/*Rect cali_roi = Pk.getCaliRegion();
		if (cali_roi.x != -1 && cali_roi.y != -1 && cali_roi.width != 0 && cali_roi.height != 0) {
			rectangle(imgcolorin, cali_roi, colors::yy, 0.3);
		}*/

		// Draw the target point the pick is centering to
		Point target_pk = Pk.getTargetPt();
		if (target_pk.x != -1 && target_pk.y != -1) {
			drawCross(imgcolorin, Pk.getTargetPt(), colors::yy, 2, 4);
		}

		// Draw the real point the pick moved to
		if (Pk.real_pt.x != -1 && Pk.real_pt.y != -1) {
			drawCross(imgcolorin, Pk.real_pt, colors::bb, 2, 4);
		}

		if (Pk.adj_pt.x != -1 && Pk.adj_pt.y != -1) {
			drawCross(imgcolorin, Pk.adj_pt, colors::gg, 2, 4);
		}

		// Overlapped FOV between main and fluo camera
		// rectangle(imgcolorin, WF.ovlpFov, colors::bb, 1);

		// Draw the cross of object that has the highest probability of being a worm
		// drawCross(imgcolorin, WF.max_prob_worm_centroid, colors::yy, 2, 8);

		// Draw the cross of object that has highest energy function
		//if (WF.worm_finding_mode == 2 && WF.max_energy_centroid != Point(-1, -1)) {
		//	drawCross(imgcolorin, WF.max_energy_centroid, colors::yy, 2, 8);
		//}
		
		if (WF.worm_finding_mode == WF.TRACK_ALL) {
			
			for (Worm w : WF.all_worms) {
				Point draw_loc = Point2d(w.getWormCentroidGlobal().x * w_multiplier, w.getWormCentroidGlobal().y * h_multiplier);
				if(w.is_lost)
					drawCross(imgcolorin, draw_loc, colors::rr, 1, 5);
				else
					drawCross(imgcolorin, draw_loc, colors::gg, 1, 5);
				Rect draw_roi = Rect(w.getWormRoi().x*w_multiplier, w.getWormRoi().y*h_multiplier, w.getWormRoi().width*w_multiplier, w.getWormRoi().height*h_multiplier);
				rectangle(imgcolorin, draw_roi, colors::yy, 1);
			}
		}

		if (WF.worm_finding_mode == WF.TRACK_ALL || WF.worm_finding_mode == WF.TRACK_ONE) {
			Point pick_reg_center = Point(WF.pickable_region.pickable_center.x * w_multiplier, WF.pickable_region.pickable_center.y * h_multiplier);
			//Rect draw_pick_region_roi = Rect(WF.pickable_region.x * w_multiplier, WF.pickable_region.y * h_multiplier, WF.pickable_region.width * w_multiplier, WF.pickable_region.height * h_multiplier);
			//rectangle(imgcolorin, draw_pick_region_roi, colors::rr, 1);
			circle(imgcolorin, pick_reg_center, WF.pickable_region.radius * w_multiplier, colors::pp, 1);
			drawCross(imgcolorin, pick_reg_center, colors::pp, 1, 8);
		}

		// Draw tracked centroid and ROI
		if (WF.getTrackedCentroid() != Point(-1, -1) && (WF.worm_finding_mode == WF.TRACK_ALL || WF.worm_finding_mode == WF.TRACK_ONE)) {
			//cout << WF.getTrackedCentroid().x << ", " << WF.getTrackedCentroid().y << endl;
			
			Point draw_loc = Point2d(WF.getTrackedCentroid().x * w_multiplier, WF.getTrackedCentroid().y * h_multiplier);
			//cout << w_multiplier << ", " << h_multiplier << endl;
			//drawCross(imgcolorin, Point(300, 300), colors::bb, 3, 30);
			drawCross(imgcolorin, draw_loc, colors::bb, 1, 5);
		}

		Rect Roi = WF.getTrackedRoi();
		if (Roi.x != -1 && Roi.y != -1 && (WF.worm_finding_mode == WF.TRACK_ALL || WF.worm_finding_mode == WF.TRACK_ONE)) {
			Rect draw_roi = Rect(Roi.x*w_multiplier, Roi.y*h_multiplier, Roi.width*w_multiplier, Roi.height*h_multiplier);
			rectangle(imgcolorin, draw_roi, colors::yy, 1); 
		}

		// Draw center of High Mag FOV
		if (!WF.verifying_pick_action) {
			Point draw_fov_loc = Point2d(WF.high_mag_fov_center_loc.x * w_multiplier, WF.high_mag_fov_center_loc.y * h_multiplier);
			drawCross(imgcolorin, draw_fov_loc, colors::rr, 1, 10);
		}

		if (WF.verifying_pick_action) {
			//cout << "display verify circle" << endl;
			Point center(WF.getPickupLocation().x * w_multiplier, WF.getPickupLocation().y * h_multiplier);
			circle(imgcolorin, center, WF.verify_radius_near_pick * w_multiplier, colors::yy, 1);
		}

		////Draw the tracked contour in a different color
		//if (!WF.getTrackedContour().empty()) {
		//	vector<vector<Point>> temp;
		//	temp.push_back(WF.getTrackedContour());
		//	drawContours(imgcolorin, temp, -1, drawcolor, 1, 8);
		//}


		// Draw key points
		if (!WF.verifying_pick_action) {
			Point center(imgcolorin.cols / 2, imgcolorin.rows / 2);
			Point center_tl(imgcolorin_tl.cols / 2, imgcolorin_tl.rows / 2);
			//circle(imgcolorin, WF.getTrackedCentroid(), (int)WF.track_radius, drawcolor, drawthickness);
			//drawCross(imgcolorin, WF.getPickupLocation(), drawcolor, drawthickness, (int)(WF.track_radius / 2.0));
			drawCross(imgcolorin, center, colors::ww, 1, (int)(WF.track_radius / 2.0));/// Absolute image center
			drawCross(imgcolorin_tl, center_tl, colors::ww, 2, 25);// Draw the center of high-mag image
		}


		// Draw pick-up location
		/*if (WF.getPickupLocation().x != -1 && WF.getPickupLocation().y != -1) {
			drawCross(imgcolorin, WF.getPickupLocation(), colors::rr, 2, 8);
		}*/
		


		/*if (WF.watching_for_picked > 0) {
			circle(imgcolorin, center, WF.diff_search_ri, pickedsearchcolor);
			circle(imgcolorin, center, WF.diff_search_ro, pickedsearchcolor);
		}


		if (WF.watching_for_picked > 0) {
			circle(imgcolorin, center, WF.diff_search_ri, pickedsearchcolor);
			circle(imgcolorin, center, WF.diff_search_ro, pickedsearchcolor);
		}*/


		// Draw pick
		//for (int i = 0; i < std::max((int)Pk.getPickCoords().size() - 1, 0); i++) {
		//	line(imgcolorin, Pk.getPickCoords()[i], Pk.getPickCoords()[i + 1], colors::gg, 1);
		//}

		// Draw pick end location IF in bounds
		if (Pk.pickTipVisible()) {
			//cout << "pickTipVisible" << endl;
			Point draw_tip = Point(Pk.getPickTip().x * w_multiplier, Pk.getPickTip().y * h_multiplier);
			drawCross(imgcolorin, draw_tip, colors::gg, 2, 20);
		}


		//// Draw the ROI for pick finding
		if (Pk.Pick_ROI.x != -1 && Pk.Pick_ROI.y != -1) {
			Rect draw_pick_roi = 
				Rect(Pk.Pick_ROI.x * w_multiplier, 
					Pk.Pick_ROI.y * h_multiplier,
					Pk.Pick_ROI.width * w_multiplier,
					Pk.Pick_ROI.height * h_multiplier);
			rectangle(imgcolorin, draw_pick_roi, colors::gg, 0.5);

		}


		// Draw latest high magnification segmentation 
		/*if (WF.getTrackedWorm() != nullptr && WF.getTrackedWorm()->getContour().size() > 0) {
			WF.getTrackedWorm()->drawSegmentation(imgcolorin_tl);
		}*/

	}

	// Display overlay image with contours

	cv::namedWindow("Figure 2", WINDOW_AUTOSIZE);
	cv::imshow("Figure 2", imgcolorin_tl);

	cv::namedWindow("Figure 1", WINDOW_AUTOSIZE);
	cv::imshow("Figure 1", imgcolorin);



	//imwrite("Figure 1.png", imgcolorin);

	// Display segmented image
	//namedWindow("Figure 2", WINDOW_AUTOSIZE);
	//imshow("Figure 2", bwin);
	//imwrite("Figure 2.png", bwin);
	int key = waitKeyEx(1);

	// Only overwrite k if it hasn't been used yet (otherwise user might LOSE keypresses)
	if (k < 0) { k = key; }

	return;
}

void display_image(int & k, cv::Mat & imgcolorin, const cv::Mat & bwin, const WormFinder& worms, const std::string & message, const bool & wormSelectMode)
{

	// Determine colors to use
	Scalar drawcolor, rectcolor, pickedsearchcolor;
	Scalar wormcolor = colors::yy;
	int drawthickness;

	if (wormSelectMode) {
		wormcolor = colors::pp;
		drawcolor = colors::yy;
		drawthickness = 2;
	}
	else if (worms.isWormOnPickup()) {
		drawcolor = colors::gg; drawthickness = 2;
	}
	else if (worms.isWormTracked()) {
		drawcolor = colors::bb; drawthickness = 1;
	}
	else {
		drawcolor = colors::rr; drawthickness = 1;
	}

	pickedsearchcolor = colors::rr;
	rectcolor = colors::kk;

	// Only draw overlays if requested
	if (worms.overlayflag) {
		//cout << "Reached" << endl;
		// Draw text message on color image
		Size sz = imgcolorin.size();
		rectangle(imgcolorin, Rect(1, 1, sz.width, 30), rectcolor, -1);
		putText(imgcolorin, message, Point(10, 22), 1, 1, colors::yy, 1);

		// Draw the final contours on the image
		//drawContours(imgcolorin, WF.getWormContours(), -1, wormcolor, 1, 8);

		// Draw the circles of WF according to centroids generated by CNN
		if (!worms.all_worms.empty()) {
			cout << "Printing" << endl;
			for (int i = 0; i < worms.all_worms.size(); i++) {
				circle(imgcolorin, worms.all_worms[i].getWormCentroidGlobal(), 5, colors::bb, 5); // PeteQ: I'm assuming the coordinates that need to be passed here are the global coordinates?
			}
		}

		if (!worms.HighEnergyCentroids.empty()) {
			for (int i = 0; i < worms.HighEnergyCentroids.size(); i++) {
				drawCross(imgcolorin, worms.HighEnergyCentroids[i], colors::rr, 2, 8);
			}
		}

		if (!worms.Monitored_ROIs.empty()) {
			for (int i = 0; i < worms.Monitored_ROIs.size(); i++) {
				rectangle(imgcolorin, worms.Monitored_ROIs[i], colors::rr, 2);
			}
		}

		// Draw the cross of object that has the highest probability of being a worm
		// drawCross(imgcolorin, WF.max_prob_worm_centroid, colors::yy, 2, 8);

		// Draw the cross of object that has highest energy function
		/*if (WF.max_energy_centroid != Point(-1, -1)) {
			drawCross(imgcolorin, WF.max_energy_centroid, colors::yy, 2, 8);
		}*/

		// Draw tracked centroid and ROI
		if (worms.getTrackedCentroid() != Point(-1, -1)) {
			drawCross(imgcolorin, worms.getTrackedCentroid(), colors::yy, 2, 8);
		}
		Rect Roi = worms.getTrackedRoi();
		if (Roi.x != -1 && Roi.y != -1) {
			rectangle(imgcolorin, Roi, colors::yy, 2);
		}

		////Draw the tracked contour in a different color
		//if (!WF.getTrackedContour().empty()) {
		//	vector<vector<Point>> temp;
		//	temp.push_back(WF.getTrackedContour());
		//	drawContours(imgcolorin, temp, -1, drawcolor, 1, 8);
		//}


		// Draw key points
		Point center(imgcolorin.cols / 2, imgcolorin.rows / 2);
		//circle(imgcolorin, WF.getTrackedCentroid(), (int)WF.track_radius, drawcolor, drawthickness);
		//drawCross(imgcolorin, WF.getPickupLocation(), drawcolor, drawthickness, (int)(WF.track_radius / 2.0));
		drawCross(imgcolorin, center, colors::ww, 2, (int)(worms.track_radius / 2.0));/// Absolute image center

		// Draw pick-up location
		drawCross(imgcolorin, Point(390, 215), colors::rr, 5, 8);


		if (worms.watching_for_picked > 0) {
			circle(imgcolorin, center, worms.diff_search_ri, pickedsearchcolor);
			circle(imgcolorin, center, worms.diff_search_ro, pickedsearchcolor);
		}


		if (worms.watching_for_picked > 0) {
			circle(imgcolorin, center, worms.diff_search_ri, pickedsearchcolor);
			circle(imgcolorin, center, worms.diff_search_ro, pickedsearchcolor);
		}


		// Draw pick
		//for (int i = 0; i < std::max((int)Pk.getPickCoords().size() - 1, 0); i++) {
		//	line(imgcolorin, Pk.getPickCoords()[i], Pk.getPickCoords()[i + 1], colors::gg, 1);
		//}

		// Draw pick end location IF in bounds
	}

	// Display overlay image with contours
	namedWindow("Figure 1", WINDOW_AUTOSIZE);
	imshow("Figure 1", imgcolorin);
	//imwrite("Figure 1.png", imgcolorin);

	// Display segmented image
	//namedWindow("Figure 2", WINDOW_AUTOSIZE);
	//imshow("Figure 2", bwin);
	//imwrite("Figure 2.png", bwin);
	int key = waitKeyEx(1);

	// Only overwrite k if it hasn't been used yet (otherwise user might LOSE keypresses)
	if (k < 0) { k = key; }

	return;
}


void display_image(int & k, cv::Mat & imgcolorin, cv::Mat & imgcolorin_tl, const cv::Mat & bwin, const WormFinder& worms, const std::string & message, const bool & wormSelectMode)
{

	// Determine colors to use
	Scalar drawcolor, rectcolor, pickedsearchcolor;
	Scalar wormcolor = colors::yy;
	int drawthickness;

	if (wormSelectMode) {
		wormcolor = colors::pp;
		drawcolor = colors::yy;
		drawthickness = 2;
	}
	else if (worms.isWormOnPickup()) {
		drawcolor = colors::gg; drawthickness = 2;
	}
	else if (worms.isWormTracked()) {
		drawcolor = colors::bb; drawthickness = 1;
	}
	else {
		drawcolor = colors::rr; drawthickness = 1;
	}

	pickedsearchcolor = colors::rr;
	rectcolor = colors::kk;

	// Only draw overlays if requested
	if (worms.overlayflag) {
		//cout << "Reached" << endl;
		// Draw text message on color image
		Size sz = imgcolorin.size();
		rectangle(imgcolorin, Rect(1, 1, sz.width, 30), rectcolor, -1);
		putText(imgcolorin, message, Point(10, 22), 1, 1, colors::yy, 1);

		// Draw the final contours on the image
		//drawContours(imgcolorin, WF.getWormContours(), -1, wormcolor, 1, 8);

		// Draw the circles of WF according to centroids generated by CNN
		if (!worms.all_worms.empty()) {
			cout << "Printing" << endl;
			for (int i = 0; i < worms.all_worms.size(); i++) {
				circle(imgcolorin, worms.all_worms[i].getWormCentroidGlobal(), 5, colors::bb, 5); // PeteQ: I'm assuming the coordinates that need to be passed here are the global coordinates?
			}
		}

		if (!worms.HighEnergyCentroids.empty()) {
			for (int i = 0; i < worms.HighEnergyCentroids.size(); i++) {
				drawCross(imgcolorin, worms.HighEnergyCentroids[i], colors::rr, 2, 8);
			}
		}

		if (!worms.Monitored_ROIs.empty()) {
			for (int i = 0; i < worms.Monitored_ROIs.size(); i++) {
				rectangle(imgcolorin, worms.Monitored_ROIs[i], colors::rr, 2);
			}
		}

		// Draw the cross of object that has the highest probability of being a worm
		// drawCross(imgcolorin, WF.max_prob_worm_centroid, colors::yy, 2, 8);

		// Draw the cross of object that has highest energy function
		/*if (WF.max_energy_centroid != Point(-1, -1)) {
			drawCross(imgcolorin, WF.max_energy_centroid, colors::yy, 2, 8);
		}*/

		// Draw tracked centroid and ROI
		if (worms.getTrackedCentroid() != Point(-1, -1)) {
			drawCross(imgcolorin, worms.getTrackedCentroid(), colors::yy, 2, 8);
		}
		Rect Roi = worms.getTrackedRoi();
		if (Roi.x != -1 && Roi.y != -1) {
			rectangle(imgcolorin, Roi, colors::yy, 2);
		}

		////Draw the tracked contour in a different color
		//if (!WF.getTrackedContour().empty()) {
		//	vector<vector<Point>> temp;
		//	temp.push_back(WF.getTrackedContour());
		//	drawContours(imgcolorin, temp, -1, drawcolor, 1, 8);
		//}


		// Draw key points
		Point center(imgcolorin.cols / 2, imgcolorin.rows / 2);
		//circle(imgcolorin, WF.getTrackedCentroid(), (int)WF.track_radius, drawcolor, drawthickness);
		//drawCross(imgcolorin, WF.getPickupLocation(), drawcolor, drawthickness, (int)(WF.track_radius / 2.0));
		drawCross(imgcolorin, center, colors::ww, 2, (int)(worms.track_radius / 2.0));/// Absolute image center

		// Draw pick-up location
		drawCross(imgcolorin, Point(390, 215), colors::rr, 5, 8);


		if (worms.watching_for_picked > 0) {
			circle(imgcolorin, center, worms.diff_search_ri, pickedsearchcolor);
			circle(imgcolorin, center, worms.diff_search_ro, pickedsearchcolor);
		}


		if (worms.watching_for_picked > 0) {
			circle(imgcolorin, center, worms.diff_search_ri, pickedsearchcolor);
			circle(imgcolorin, center, worms.diff_search_ro, pickedsearchcolor);
		}


		// Draw pick
		//for (int i = 0; i < std::max((int)Pk.getPickCoords().size() - 1, 0); i++) {
		//	line(imgcolorin, Pk.getPickCoords()[i], Pk.getPickCoords()[i + 1], colors::gg, 1);
		//}

		// Draw pick end location IF in bounds
	}

	// Display overlay image with contours
	namedWindow("Figure 1", WINDOW_AUTOSIZE);
	imshow("Figure 1", imgcolorin);
	//imwrite("Figure 1.png", imgcolorin);

	namedWindow("Figure 2", WINDOW_AUTOSIZE);
	imshow("Figure 2", imgcolorin_tl);


	// Display segmented image
	//namedWindow("Figure 2", WINDOW_AUTOSIZE);
	//imshow("Figure 2", bwin);
	//imwrite("Figure 2.png", bwin);
	int key = waitKeyEx(1);

	// Only overwrite k if it hasn't been used yet (otherwise user might LOSE keypresses)
	if (k < 0) { k = key; }

	return;
}


void displayImageThreadFunc(int& k, Mat &imgcolorin, Mat &imgcolorin_tl, const Mat &bwin, const Mat &bwin_tl, const WormPick& Pk,
	 const WormFinder& worms, const string &message, const bool &wormSelectMode) {

	for (;;) {
		boost_interrupt_point();
		boost_sleep_ms(5);
		display_image(k, imgcolorin, imgcolorin_tl, bwin, bwin_tl, Pk, worms, message, wormSelectMode);
	}

}

int optimize_threshold(const Mat &img, WormFinder& worms, HardwareSelector &use, ErrorNotifier &Err) {

	// Declare variables
	Mat bw, imgwork, imgcolor;
	vector<vector<Point>> contours;
	char displaytext[300];
	bool ipablank = false;
	WormPick PkBlank(ipablank, use, Err);

	// Open the output file to save the threshold
	FILE* fThresh;
	fopen_s(&fThresh, "params_thresh.txt", "w");

	// Generate threshold with most valid WF
	int max_N = 0;
	int max_idx = 0;

	for (int i = -1; i < use.img_max; i++) {

		// Reset the image
		img.copyTo(imgwork);

		// Get basic segmentation
		segment_image(i, imgwork, bw, worms.getWormContours(), use);

		// Extract temporary WF with specific parameters
		worms.extractBlobs();

		// Store info on the threshold that was used
		worms.thresh = i;

		// Evaluate results
		worms.evaluateBlobs(imgwork);

		// Display image
		sprintf_s(displaytext, "thr=%d | Nw=%d | Wavg=%0.2f | Esc", worms.thresh, worms.getNumBlobs(), worms.evalmetrics[2]);
		cvtColor(bw, imgcolor, COLOR_GRAY2RGB);
		int k = -5;
		//int k = display_image(30, imgcolor, bw, false, PkBlank,  WF, displaytext);

		// Check if this is the optimal threshold
		if (worms.getNumBlobs() >= max_N && worms.evalmetrics[2] < worms.contrast_max) {
			max_N = worms.getNumBlobs();
			max_idx = i;
		}

		// Exit if requested
		if (k == 27) {
			break;
		}
	}

	// Valid threshold discovered: Display image and return the threshold
	if (max_N > 0) {
		printf("Threshold %d had the most worms (%d)\n", max_idx, max_N);
		worms.thresh = max_idx;
		segment_image(worms.thresh, imgwork, bw, contours, use);
		worms.extractBlobs();
		worms.evaluateBlobs(img);
		sprintf_s(displaytext, "SET: thr=%d | Nw=%d | Wavg=%0.2f | Esc", worms.thresh, worms.getNumBlobs(), worms.evalmetrics[2]);
		//display_image(0, imgcolor, bw, false, PkBlank, WF, displaytext);;

		// Save threshold to disk
		fprintf(fThresh, "%d\n", worms.thresh);
		fclose(fThresh);

		// Return the threshold
		return worms.thresh;
	}

	// No valid threshold discovered: Display error and return negative
	else {
		printf("Failed to find any valid thresholds - Set to 0\n");
		sprintf_s(displaytext, "NO VALID THRESHOLD FOUND!");
		//display_image(0, imgcolor, bw, false, PkBlank, WF, displaytext);
		worms.thresh = -1;
		return -1;
	}
}



void writeMetaData(double frametime, ofstream& out, const WormFinder& worms, const OxCnc& Ox, const OxCnc& Grbl2, const WormPick& Pk) {


	out << frametime << "\t";					/// Column 1: Timestamp of the current frame
	out << worms.isWormTracked() << "\t";			/// Column 2: Whether worm tracking is on
	out << worms.thresh << "\t";				/// Column 3: Image threshold
	out << worms.getPickupLocation().x << "\t";		/// Column 4: Pick contact point, x coordinate 
	out << worms.getPickupLocation().y << "\t";		/// Column 5: Pick contact point, y coordinate
	out << worms.getNumBlobs() << "\t";				/// Column 6: Number of worm-like blobs
	out << worms.getTrackedCentroid().x << "\t";	/// Column 7: Centroid of currently track worm, x coordinate
	out << worms.getTrackedCentroid().y << "\t";	/// Column 8: Centroid of currently track worm, y coordinate
	out << Ox.getX() << "\t";					/// Column 9: Ox CNC, X coordinate
	out << Ox.getY() << "\t";					/// Column 10: Ox CNC, Y coordinate
	out << Ox.getZ() << "\t";					/// Column 11: Ox CNC, Z coordinate
	out << Grbl2.getX() << "\t";				/// Column 12: Grbl2, X coordinate
	out << Grbl2.getY() << "\t";				/// Column 13: Grbl2, Y coordinate
	out << Grbl2.getZ() << "\t";				/// Column 14: Grbl2, Z coordinate
	out << Pk.getPickGradient(true) << "\t";	/// Column 15: Pick gradient (TENV value, filtered iamge ROI)
	out << Pk.getPickGradient(false) << "\t";	/// Column 16: Pick gradient (TENV value, unfiltered image ROI)
	out << Pk.getPickTip().x << "\t";			/// Column 17: Pick tip, X coordinate
	out << Pk.getPickTip().y << "\t";			/// Column 18: Pick tip, Y coordinate
	out << Pk.in_pick_action << "\t";			/// Column 19: Whether currently in a pick action
	out << Pk.action_success_user << "\t";		/// Column 20: Manual indication of success or failure by 1/0
	out << Pk.action_success << "\t";			/// Column 21: Automatic indication of success or failure
	out << endl;

}

/*
	Compute TENV (
	grad variance) focus metric

	Adapted from:
	https://stackoverflow.com/questions/7765810/is-there-a-way-to-detect-if-an-image-is-blurry
*/

double tenengrad(const cv::Mat& src, int ksize, double resize_factor) {

	// Resize the image to speed up computation if requested
	Mat Gx, Gy, src_resized;

	// TODO fix exception
	resize(src, src_resized, Size(0, 0), resize_factor, resize_factor, 0);

	// Compute sobel X and Y gradients
	Sobel(src_resized, Gx, CV_64F, 1, 0, ksize);
	Sobel(src_resized, Gy, CV_64F, 0, 1, ksize);

	Mat FM = Gx.mul(Gx) + Gy.mul(Gy);

	Scalar mu, sigma;
	meanStdDev(FM, mu, sigma);

	double focusMeasure = sigma.val[0] * sigma.val[0];
	return focusMeasure / 1000.0;
}

// measure TENV around a sepcific point, e.g. the tip, using standard parameters
//		Point pt is assumed to lie on the center of the right boundary of the cubic ROI
double tenengrad(const Mat& src, Point pt, int radius) {

	// Set standard parameters
	int ksize = 3;
	double resize_factor = 1;
	Size sz = src.size();
	int wi = sz.width;
	int hi = sz.height;

	// Set image ROI, force in bounds
	int x0 = max(1, pt.x - 2 * radius);
	int y0 = max(1, pt.y - radius);
	int w = min(2 * radius + 1, wi - x0 - 1);
	int h = min(2 * radius + 1, hi - y0 - 1);
	Rect r(x0, y0, w, h);

	// Feed ROI image to tenengrad
	Mat src_roi = src(r);
	return tenengrad(src_roi, ksize, resize_factor);
}

// Measure INTENSITY around a specific point, same ROI method as TENENGRAD
//		Point pt is assumed to lie on the center of the right boundary of the cubic ROI
double localIntensity(const Mat& src, Point pt, int radius) {

	// Set standard parameters
	Size sz = src.size();
	int wi = sz.width;
	int hi = sz.height;

	// Set image ROI, force in bounds
	int x0 = max(1, pt.x - 2 * radius);
	int y0 = max(1, pt.y - radius);
	int w = min(2 * radius + 1, wi - x0 - 1);
	int h = min(2 * radius + 1, hi - y0 - 1);
	Rect r(x0, y0, w, h);

	// Measure the mean value of the region
	Mat src_roi = src(r);
	Scalar mean_val = mean(src_roi);
	return mean_val[0];

}


// Focus image and then move OX-Z slightly up OOF
void focus_image(OxCnc& Ox, WormPick& Pk, WormFinder& worms, int isLowmag) {

	cout << "focusing image\n";
	bool continue_focusing = true;
	vector<double> image_focus;
	double change_increment, img_gradient_diff = 0, img_gradient_diff_thresh = 0;
	if (isLowmag) {change_increment = 1;}
	else { change_increment = 0.25; }
	int direction = 1;
	double upper_limit = Ox.getZ() + 2.5; /// 5 mm range around current focus
	double lower_limit = Ox.getZ() - 2.5; /// 5 mm below current focus

	while (continue_focusing) {

		/// If we're just starting the lowering, then seed the focus curve with the current tenengrad
		///		Note that if pick isn't visible, then the gradient value is measured near the right 
		///		of the image
		if (image_focus.size() == 0)
		{
			if (isLowmag) { image_focus.push_back(tenengrad(worms.imggray(Pk.getCaliRegion()), 3, 0.5)); }
			else { image_focus.push_back(tenengrad(worms.img_high_mag, 3, 0.5)); }
		}

		/// Move Down unless we reach the limit of motion
		if (!Ox.goToRelative(0, 0, change_increment*direction)) {
			if (direction == 1)
				direction = -1;
			else
				break;
		}
		//boost_sleep_ms(450);
		boost_sleep_ms(150);

		/// Record Pick focus metric locally until it decreases. If it seems to decrease, 
		/// wait a moment and triple check to confirm it wasn't a movement / vibration artefact
		int tries = 0;
		if (isLowmag) {
			while (tenengrad(worms.imggray(Pk.getCaliRegion()), 3, 0.5) < image_focus[image_focus.size() - 1] && tries < 3) {
				tries += 1;
				//boost_sleep_ms(250 * tries);
				boost_sleep_ms(100 * tries);
			}
		}
		else {
			while (tenengrad(worms.img_high_mag, 3, 0.5) < image_focus[image_focus.size() - 1] && tries < 3) {
				tries += 1;
				//boost_sleep_ms(250 * tries);
				boost_sleep_ms(100 * tries);
			}
		}

		double this_tenv;
		if (isLowmag) { this_tenv = tenengrad(worms.imggray(Pk.getCaliRegion()), 3, 0.5); }
		else { this_tenv = tenengrad(worms.img_high_mag, 3, 0.5); }
		
		image_focus.push_back(this_tenv);
		cout << "TENV = "  << this_tenv << endl;
		int idx = (int)image_focus.size() - 1;
		img_gradient_diff = image_focus[idx] - image_focus[idx - 1];

		/// If TENV has decreased and we're going in the second direction,
		///		OR we have reached the lower limit, finish
		if (img_gradient_diff < img_gradient_diff_thresh && direction == -1 ||
			Ox.getZ() <= lower_limit) {
			continue_focusing = false;
		}

		/// If TENV has decreased and we're going in the first direction,
		///		OR we have reached the upper limit, switch
		else if (img_gradient_diff < img_gradient_diff_thresh && direction == 1 ||
			Ox.getZ() >= upper_limit) {
			continue_focusing = true;
			direction = -1;
		}

		/// Note that the direction will also change or exit if we reach the 
		///		soft limits for OX Z motion.
		boost_interrupt_point();
	}

	/// Go back up one unit, this is the "peak" focus
	Ox.goToRelative(0, 0, change_increment * 1.4);

	/// Move up additional 1 mm to bring image slightly OOF
	//Ox.goToRelative(0, 0, change_increment * 1);

}


void CallBackFunc(int event, int x, int y, int flags, void* userdata) {

	WormFinder* w = (WormFinder*)userdata;

	if (event == EVENT_LBUTTONDOWN) {
		cout << "Left mouse button clicked" << endl;
		// look for a new worm at the click location
		cout << "x = " << x << ", y = " << y << endl;
		w->findOnClick(x, y);
		w->SetMoveOnClick(x, y);
	}
	else if (event == EVENT_RBUTTONDOWN) {
		w->setPickupLocation(x, y);
	}
}
