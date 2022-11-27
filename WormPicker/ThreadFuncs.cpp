#pragma once
// OpenCV includes
#include "opencv2\opencv.hpp"


// Standard includes
#include <cstdio>
#include <iostream>
#include <string>

// Anthony's includes
#include "ThreadFuncs.h"
#include "AnthonysColors.h"
#include "ImageFuncs.h"
#include "WormFinder.h"
#include "AnthonysTimer.h"
#include "LabJackFuncs.h"
#include "PlateTray.h"
#include "FileNames.h" 

// Boost includes
#include "boost\asio.hpp"
#include "boost\thread.hpp"

using namespace std;
using namespace cv;


// If signalled, write the new frame to disk. If not, sleep.
int WriteFrameToDisk_raw(VideoWriter &writer, Mat *imgwrite, std::shared_ptr<int> readyflag) {
	//int i = 0;
	//cout << "Inside writing raw image thread" << endl;
	for (;;) {
		//cout << "Checkpoint of readyflag->" << *readyflag << endl;
		if (*readyflag == 1) {

			writer.write(*imgwrite); 
			//cout << "Wrote frame #" << i++ << " rows: " << imgwrite->rows << " cols: " << imgwrite->cols << endl;
			*readyflag = 0;
			//cout << "Writing raw thread->" << *readyflag << endl;
		}
		// Pause for a few ms
		boost::this_thread::sleep(boost::posix_time::milliseconds(2));

		/* Check if the thread has been marked for interruption */
		boost::this_thread::interruption_point();
	}

	//cout << "Exit writing raw image thread" << endl;
	return 0;
}

int WriteFrameToDisk_color(VideoWriter &writer, Mat *imgwrite, std::shared_ptr<int> readyflag) {
	//int i = 0;
	for (;;) {
		if (*readyflag == 1) {

			writer.write(*imgwrite);
			//cout << "Wrote frame #" << i++ << " rows: " << imgwrite->rows << " cols: " << imgwrite->cols << endl;
			*readyflag = 0;
			//cout << "Writing color thread->" << *readyflag << endl;
		}
		// Pause for a few ms
		boost::this_thread::sleep(boost::posix_time::milliseconds(2));

		/* Check if the thread has been marked for interruption */
		boost::this_thread::interruption_point();
	}

	return 0;
}

int WriteFrameToDisk_tl(cv::VideoWriter &writer, cv::Mat *imgwrite, std::shared_ptr<int> readyflag) {

	//int i = 0;
	for (;;) {
		if (*readyflag == 1) {

			writer.write(*imgwrite);
			//cout << "Wrote frame #" << i++ << " rows: " << imgwrite->rows << " cols: " << imgwrite->cols << endl;
			*readyflag = 0;
			//cout << "Writing color thread->" << *readyflag << endl;
		}
		// Pause for a few ms
		boost::this_thread::sleep(boost::posix_time::milliseconds(2));

		/* Check if the thread has been marked for interruption */
		boost::this_thread::interruption_point();
	}

	return 0;
}

int WriteFrameToDisk_color_with_tl(VideoWriter &writer, Mat *imgwrite_color, Mat *imgwrite_tl, std::shared_ptr<int> readyflag) {
	//int i = 0;
	for (;;) {
		if (*readyflag == 1) {

			Size color_s = imgwrite_color->size();
			Size tl_s = imgwrite_tl->size();

			Size total_s = Size(imgwrite_color->size().width + imgwrite_tl->size().width, max(imgwrite_color->size().height, imgwrite_tl->size().height));

			Mat stitched_img(total_s, imgwrite_color->type(), Scalar(255, 255, 255));

			int row_start = (total_s.height - color_s.height) / 2;
			/*int col_start = ((total.width - high_s.width) - low_s.width) / 2;

			cout << "Row start: " << row_start << endl;
			cout << "Col start: " << col_start << endl;

			cout << "Row end: " << row_start + low_s.height << endl;
			cout << "Col end: " << col_start + low_s.width << endl;*/

			imgwrite_color->copyTo(stitched_img.rowRange(row_start, row_start + color_s.height).colRange(0, color_s.width));

			//cout << "Size: " << total.width << ", " << total.height << endl;

			row_start = (total_s.height - tl_s.height) / 2;
			//col_start = low_s.width + (total.width - high_s.width) / 2;

			//cout << "Row start: " << row_start << endl;
			//cout << "Col start: " << col_start << endl;

			//cout << "Row end: " << row_start + low_s.height << endl;
			//cout << "Col end: " << col_start + low_s.width << endl;

			imgwrite_tl->copyTo(stitched_img.rowRange(row_start, row_start + tl_s.height).colRange(color_s.width, total_s.width));

			writer.write(stitched_img);
			//cout << "Wrote frame #" << i++ << " rows: " << imgwrite->rows << " cols: " << imgwrite->cols << endl;
			*readyflag = 0;
			//cout << "Writing color thread->" << *readyflag << endl;
		}
		// Pause for a few ms
		boost::this_thread::sleep(boost::posix_time::milliseconds(2));

		/* Check if the thread has been marked for interruption */
		boost::this_thread::interruption_point();
	}

	return 0;

}


// Thread function for reading/Writing from DAC or FIO (not AIN)
//void LabJackHandler(LJ_HANDLE lngHandle, int *dac_channel, int *fio_channel, double *request_value, bool *read_flag) {
//
//	for (;;) {
//
//		// Check for exit request
//		boost_sleep_ms(25);
//		boost_interrupt_point();
//		long state = 0;
//
//		// Take no action if no port is requested OR if multiple ports are requested (by mistake)
//		if (*dac_channel < 0 && *fio_channel < 0) {
//			continue;
//		}
//		if (*dac_channel >= 0 && *fio_channel >= 0) {
//			cout << "Actions were simultaneously requested on channels " << dac_channel << " and " << fio_channel
//				<< ". No action was taken\n";
//			continue;
//		}
//
//		// Route request appropriately
//
//		/// Write to dac channel
//		if (*dac_channel>=0 && !*read_flag)
//			LabJack_SetDacN(lngHandle, *dac_channel, *request_value);
//
//		/// Write to FIO channel
//		else if (*fio_channel >= 0 && !*read_flag)
//			LabJack_SetFioN(lngHandle, *dac_channel, (int)*request_value);
//
//		/// Read from FIO channel
//		else if (fio_channel >= 0 && *read_flag) {
//			LabJack_ReadFioN(lngHandle, *dac_channel, state);
//			*request_value = state;
//		}
//	}
//}



// Helpers
void boost_sleep_ms(int ms) {
	boost::this_thread::sleep(boost::posix_time::milliseconds(ms));
}

void boost_sleep_us(long us) {
	boost::this_thread::sleep(boost::posix_time::microseconds(us));
}

void boost_interrupt_point() {
	boost::this_thread::interruption_point();
}
