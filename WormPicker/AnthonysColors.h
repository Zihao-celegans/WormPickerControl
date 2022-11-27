/*
 *		AnthonysColors.h
 *		Anthony Fouad
 *		Fang-Yen Group, 6/2014
 *
 *		Contains a series of MATALB-like color definitions for quick use with OPENCV.
 */
#pragma once
#ifndef ANTHONYSCOLORS_H_
#define ANTHONYSCOLORS_H_

#include "opencv2/opencv.hpp"

namespace colors {
	const cv::Scalar bb(65500, 0, 0);
	const cv::Scalar gg(0, 65500, 0);
	const cv::Scalar yy(0, 65500, 65500);
	const cv::Scalar kk(0, 0, 0);
	const cv::Scalar pp(65500, 0, 65500);
	const cv::Scalar ww(65500, 65500, 65500);
	const cv::Scalar rr(0, 0, 65500);
	const cv::Scalar mm(65500 * 179 / 255, 65500 * 21 / 255, 65500);
}

#endif

