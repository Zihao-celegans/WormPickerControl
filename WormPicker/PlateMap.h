/*
	PlateMap.h
	Alexander Kassouni
	Fang-Yen Lab
	Summer 2019

	datastruct for keeping track of where the robot has searched on a plate

	The plate is represented by a matrix. At each entry of the matrix (corresonding to once FOV length of the camera),
	the entry can be OOB, UNSEARCHED, POPULATED, or UNPOPULATED

		--> FOV size much be specified, in CNC movement units (e.g. mm)
		--> Plate diameter
		--> Center coordinates of the plate

	OOB - the entry is outside of the plate (the plate is circular and the matrix is square)
		- OOB entries are calulated by the radius of the plate
	UNSEARCHED - the entry has not been searched for worms
	POPULATED - the entry has worms
	UNPOPULATED - the entry does not have worms

	When deciding which entry to go to, the robot first looks for POPULATED cells, starting in the center of the plate and
	spiraling out. if none were found, the robot then looks for unsearch cells to go to.

	

*/

#pragma once
#include <cmath>
#include <iostream>

#include "boost/numeric/ublas/matrix.hpp"
#include "opencv2/opencv.hpp"

#include "OxCnc.h"
#include "PlateTray.h"
#include "MatIndex.h"
#include "showDebugImage.h"

class SpiralIterator {
public:
	SpiralIterator(int matSize) :
		X(matSize), Y(matSize), x(0), y(0), dx(0), dy(-1), t(std::max(X, Y))
	{
		
	}

	~SpiralIterator() {}
	MatIndex nextIndex() {
		MatIndex newIdx;
		if ((-X / 2 <= x) && (x <= X / 2) && (-Y / 2 <= y) && (y <= Y / 2)) {
			newIdx.row = y + Y / 2;
			newIdx.col = x + X / 2;
		}
		if ((x == y) || ((x < 0) && (x == -y)) || ((x > 0) && (x == 1 - y))) {
			t = dx;
			dx = -dy;
			dy = t;
		}
		x += dx;
		y += dy;
		return newIdx;
	}
private:
	int X, Y;
	int x, y, dx, dy, t;
};


class PlateMap
{
public:

	enum search_state { OOB, UNSEARCHED, UNPOPULATED, POPULATED };
	enum scan_mode { MAPPING, FIND_WORM };
	
	PlateMap(float fov, float diameter, cv::Point3d center);
	~PlateMap();

	void updateMap(cv::Point3d position, search_state state);
	cv::Point3d getDestination();
	void print(std::ostream& out);

	std::vector<std::pair<cv::Point3d, MatIndex>> getScanCoordPair() const;

	void CalScanPoint();

	void UpdatetileMap(MatIndex idx, PlateMap::search_state state);

	// Convert a 3D position to an index on the platemap or vice versa
	MatIndex posToIndex(cv::Point3d point);
	cv::Point3d indexToPos(MatIndex idx);

	void showPlateMap(std::vector<cv::Point> worms_coord, std::vector<double> worms_area, cv::Size FOV_sz, cv::Size dst_sz);

	cv::Point3d getRegionContainWorm(); // Get the region containing worms (if multiple regions have worms, return the one closest to the center)

private:
	// the matrix of tiles for the plate
	boost::numeric::ublas::matrix<search_state> tileMap;
	cv::Point3d center; // the center of the plate
	//std::vector<cv::Point3d> ScanPoints;
	std::vector<std::pair<cv::Point3d, MatIndex>> pr_pts_idx;
	float FOV; // Diameter of FOV
	void SetScanPair(std::vector<cv::Point3d> pts, std::vector<MatIndex> idxs);

};