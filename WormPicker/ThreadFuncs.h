

#pragma once
#ifndef THREADFUNCS_H
#define THREADFUNCS_H

// OpenCV includes
#include "opencv2\opencv.hpp"


// Anthony's includes
#include "boost\thread.hpp"
#include "LabJackFuncs.h"


int WriteFrameToDisk_raw(cv::VideoWriter &writer, cv::Mat *imgwrite, std::shared_ptr<int> readyflag);
int WriteFrameToDisk_color(cv::VideoWriter &writer, cv::Mat *imgwrite, std::shared_ptr<int> readyflag);
int WriteFrameToDisk_tl(cv::VideoWriter &writer, cv::Mat *imgwrite, std::shared_ptr<int> readyflag);
int WriteFrameToDisk_color_with_tl(cv::VideoWriter &writer, cv::Mat *imgwrite_color, cv::Mat *imgwrite_tl, std::shared_ptr<int> readyflag);
//void LabJackHandler(LJ_HANDLE lngHandle, int *dac_channel, int *fio_channel, double *request_value, bool *read_flag);
void boost_sleep_ms(int ms); /// Sleep for a certain number of milliseconds
void boost_sleep_us(long us); /// Sleep for a certain number of microseconds
void boost_interrupt_point();/// Interruption point for continuously looping threads

#endif