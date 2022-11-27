#include "PlateMap.h"
#include "ImageFuncs.h"

using namespace boost::numeric::ublas;
using namespace std;

PlateMap::PlateMap(float fov, float diameter, cv::Point3d center):
	center(center), FOV(fov)
{
	int buffer = 2;
	int matSize = diameter / fov + buffer;
	tileMap = matrix<PlateMap::search_state>(matSize, matSize);

	/*
		the most efficient way to iterate over matrices is as follows:

		iter1 iterates over each row of the matrix (outer for loop). iter1 is a pointer to the start of the row,
		and starts at the first row

		iter2 starts at the first entry of the row, and iterates over each entry in the row (inner for loop)

		iter1 is a pointer to the row that is being iterated over. iter2 is a pointer to the enrty that is being accessed

		This is the most efficient way to iterate over matrices because it uses pointer arithmetic
	*/

	for (matrix<PlateMap::search_state>::iterator1 iter1 = tileMap.begin1(); iter1 != tileMap.end1(); iter1++) {
		for (matrix<PlateMap::search_state>::iterator2 iter2 = iter1.begin(); iter2 != iter1.end(); iter2++) {
			float dist = ptDist(cv::Point(matSize / 2, matSize / 2), cv::Point(iter2.index1(), iter2.index2()));
			if (dist > (matSize - buffer) / 2) {
				*iter2 = PlateMap::OOB;
			}
			else {
				*iter2 = PlateMap::UNSEARCHED;
			}
		}
	}
}

PlateMap::~PlateMap() {}

void PlateMap::updateMap(cv::Point3d pos, PlateMap::search_state state) {

}

template <class  A>
bool indexInBounds(MatIndex idx, matrix<A> m) {
	bool isInBound = (0 <= idx.row < m.size1()) && (0 <= idx.col < m.size2());
	return (isInBound);
}

cv::Point3d PlateMap::getDestination() {
	
	SpiralIterator iter(tileMap.size1());
	MatIndex idx = iter.nextIndex();
	while (indexInBounds(idx, tileMap)) {
		idx = iter.nextIndex();
	}

	return cv::Point3d();
}

void PlateMap::print(ostream& out) {
	
	for (matrix<PlateMap::search_state>::iterator1 iter1 = tileMap.begin1(); iter1 != tileMap.end1(); iter1++) {
		for (matrix<PlateMap::search_state>::iterator2 iter2 = iter1.begin(); iter2 != iter1.end(); iter2++) {
			out << (int)*iter2;
		}
		out << endl;
	}
}


std::vector<std::pair<cv::Point3d, MatIndex>> PlateMap::getScanCoordPair() const {
	return pr_pts_idx;
}

void PlateMap::CalScanPoint() {

	std::vector<cv::Point3d> scan_pts;
	std::vector<MatIndex> idx_scan;

	SpiralIterator iter(tileMap.size1());
	MatIndex idx = iter.nextIndex();

	MatIndex start_pt = idx;

	int count = 0;
	while (indexInBounds(idx, tileMap) && count < tileMap.size1()*tileMap.size2()) { // TO DO: check the OOB criteria

		if (tileMap(idx.row, idx.col) != PlateMap::OOB) {

			cv::Point3d this_pt;
			this_pt.x = center.x + (idx.col - start_pt.col) * FOV;
			this_pt.y = center.y - (idx.row - start_pt.row) * FOV;
			this_pt.z = center.z;

			scan_pts.push_back(this_pt);
			idx_scan.push_back(idx);
		}

		idx = iter.nextIndex();

		count++;
	}

	SetScanPair(scan_pts, idx_scan);
}

void PlateMap::UpdatetileMap(MatIndex idx, PlateMap::search_state state) {
	tileMap(idx.row, idx.col) = state;
}

MatIndex PlateMap::posToIndex(cv::Point3d point) {

	return MatIndex();
}

cv::Point3d PlateMap::indexToPos(MatIndex idx) {

	return cv::Point3d();
}

void PlateMap::showPlateMap(std::vector<cv::Point> worms_coord, std::vector<double> worms_area, cv::Size FOV_sz, cv::Size dst_sz) {
	// Synthesize a cv::Mat to represent plate map
	cv::Mat colormap (tileMap.size1(), tileMap.size2(), CV_8UC3);

	for (int i = 0; i < colormap.rows; i++) {
		for (int j = 0; j < colormap.cols; j++) {
			if (tileMap(i, j) == 0) {
				colormap.at<cv::Vec3b>(cv::Point(i, j)) = cv::Vec3b(200, 200, 200); // OOB
			}
			else if (tileMap(i, j) == 1) {
				colormap.at<cv::Vec3b>(cv::Point(i, j)) = cv::Vec3b(255, 0, 0); // Unsearch
			}
			else if (tileMap(i, j) == 2) {
				colormap.at<cv::Vec3b>(cv::Point(i, j)) = cv::Vec3b(255, 255, 255); // Unpopulated
			}
			else if (tileMap(i, j) == 3) {
				colormap.at<cv::Vec3b>(cv::Point(i, j)) = cv::Vec3b(255, 255, 255); // Populated
			}
		}
	}

	// Resize to a whole plate image
	cv::Mat scaled_colormap;
	resize(colormap, scaled_colormap, cv::Size(), FOV_sz.width, FOV_sz.height, cv::INTER_NEAREST);

	// calculate resize factors
	double fx = (double)dst_sz.width / scaled_colormap.cols;
	double fy = (double)dst_sz.height / scaled_colormap.rows;


	// Resize to destination size
	resize(scaled_colormap, scaled_colormap, dst_sz);


	for (int i = 0; i < worms_coord.size(); i++) {
		// Compute the transformed worm coordinates in the resultant image
		cv::Point2d tran_pt;
		tran_pt.x = round(fx*worms_coord[i].x);
		tran_pt.y = round(fy*worms_coord[i].y);

		// plot on the image which different color code
		cv::Scalar clr;
		if (worms_area[i] > 180) { clr = colors::rr;  }
		else if (worms_area[i] < 180 && worms_area[i] > 0) { clr = colors::bb; }
		else { clr = colors::kk; }

		circle(scaled_colormap, tran_pt, 5, clr, -1);
	}

	//string fout = "C:\\WormPicker\\PlateMap\\plateMap.png";
	//cv::imwrite(fout, colormap, { CV_IMWRITE_PNG_COMPRESSION, 0 });

	showDebugImage(scaled_colormap, false, "PlateMap", 3000, true);

	//cout << tileMap.size1() << endl << tileMap.size2() << endl;;

}


cv::Point3d PlateMap::getRegionContainWorm() {
	
	SpiralIterator iter(tileMap.size1());
	MatIndex idx = iter.nextIndex();

	MatIndex start_pt = idx;

	cv::Point3d pt (999,999,999);

	int count = 0;
	while (indexInBounds(idx, tileMap) && count < tileMap.size1()*tileMap.size2()) { // TO DO: check the OOB criteria

		if (tileMap(idx.row, idx.col) == PlateMap::POPULATED) {
			pt.x = center.x + (idx.col - start_pt.col) * FOV;
			pt.y = center.y - (idx.row - start_pt.row) * FOV;
			pt.z = center.z;

			break; // Once one region was found, break the loop
		}

		idx = iter.nextIndex();

		count++;
	}

	if (pt == cv::Point3d(999, 999, 999)) {
		cout << "The tile map does not contain any worm!" << endl;
	}

	return pt;

}



void PlateMap::SetScanPair(std::vector<cv::Point3d> pts, std::vector<MatIndex> idxs) {

	pr_pts_idx.clear();

	if (pts.size() == idxs.size()) {
		for (int i = 0; i < pts.size(); i++) {
			pr_pts_idx.push_back(make_pair(pts[i], idxs[i]));
		}
	}
	else {
		cout << "Fail to set scan coordinate pair: Unequal number of points & indexs" << endl;
	}
}