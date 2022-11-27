///*
//	Test safety of multithreading architectures
//
//	* Sharing a pointer among main and two threads for read/write leads to one thread crashing, usually main
//	* StrictLock locks entire objects
//*/
//
//// Anthony's includes
//#include "AnthonysCalculations.h"
//#include "AnthonysTimer.h"
//#include "ImageFuncs.h"
//#include "StrictLock.h"
//
//// Standard includes
//#include <cstdio>
//#include <iostream>
//#include <fstream>
//#include <string>
//#include <vector>
//
//
//// Boost includes
//#include "boost\asio.hpp"
//#include "boost\thread.hpp"
//
//
//using namespace std;
//
//class MyClass : public boost::basic_lockable_adapter<boost::mutex> {
//
//public:
//	MyClass(double *sd, ofstream &of) {
//		shared_i = sd;
//	}
//
//	void updatePtr(int thread_num) {
//		cout << "Thread " << thread_num << ": " << (*shared_i)++ << endl;
//		outfile << "Thread " << thread_num << ": " << (*shared_i)++ << endl;
//	}
//
//protected:
//	double *shared_i;
//	ofstream outfile;
//
//};
//
//void thread_fncn(int thread_num, MyClass *myObj, StrictLock<MyClass> *myObjLock) {
//
//	for (;;) {
//		//myObjLock->lock();
//		myObj->updatePtr(thread_num);
//		//myObjLock->unlock();
//
//		//boost_sleep_ms(1);
//		boost_interrupt_point();
//	}
//}
//
//
//
//void main() {
//
//	// Create output object
//	string fout = "C:\\WormWatcher\\test_log_file.txt";
//	ofstream outfile(fout);
//
//	// Start up threads with shared pointer variable
//	vector<boost::thread> threads;
//	double shared_i = 0;
//	MyClass myObj(&shared_i, outfile);
//	StrictLock<MyClass> myObjLock(myObj);
//	threads.push_back(boost::thread(&thread_fncn, 1, &myObj, &myObjLock));
//	threads.push_back(boost::thread(&thread_fncn, 2, &myObj, &myObjLock));
//
//	// Also update in the main thread
//	AnthonysTimer Ta;
//	while (Ta.getElapsedTime() < 60) {
//		//myObjLock.lock();
//		cout << "Main Thread " << ": " << shared_i++ << endl;
//		outfile << "Main Thread " << ": " << shared_i++ << endl;
//		//myObjLock.unlock();
//		//boost_sleep_ms(1);
//	}
//
//	// Close down
//	for (int i = 0; i < threads.size(); i++) {
//		threads[i].interrupt();
//		threads[i].join();
//	}
//
//	cout << "FINISHED - press ENTER to continue" << endl;
//	outfile << "FINISHED" << endl;
//	outfile.terminate();
//	cin.get();
//}