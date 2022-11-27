/*
	AnthonysTimer.cpp
	Anthony Fouad
	Fang-Yen Lab
	May 2018

	Contains class implementation of timer. See AnthonysTimer.h for notes
*/

#include "AnthonysTimer.h"
#include "boost\thread.hpp"
#include "ThreadFuncs.h"
#include <iostream>

using namespace std;

// Constructor - starts the timer
AnthonysTimer::AnthonysTimer() {
	// Start up the timer and set to zero
	setToZero();
}

AnthonysTimer::AnthonysTimer(bool test_expression, double seconds) {
	
	// Start up the timer and set to zero
	setToZero();

	// Optionally, if seconds is provided, initiate blocking for that duration
	if (seconds > 0) {
		blockForTime(test_expression, seconds);
	}
}

// Simple implementation of blocker function
void AnthonysTimer::blockForTime(bool test_expression, double seconds, double sleep_interval_ms){
	while (test_expression && diff < seconds) { 
		getElapsedTime();
		boost_sleep_ms(sleep_interval_ms);
	}
}

// Update time difference - can be used for custom implementations of a blocker. Also returns elapsed time
// Returns the elapsed time in seconds.
double AnthonysTimer::getElapsedTime() {
	tend = clock();  /// Update the clock
	diff = (((double)tend - (double)tstart) / 1000000.0F) * 1000; /// Calculate elapsed time
	//std::cout << diff << "s" << std::endl;
	return diff; /// Also return the elapsed time
}

double AnthonysTimer::getElapsedTimeThreadSafe() const {
	time_t tend = clock();  /// Update the clock
	time_t diff = (((double)tend - (double)tstart) / 1000000.0F) * 1000; /// Calculate elapsed time
	//std::cout << diff << "s" << std::endl;
	return diff; /// Also return the elapsed time
}

// Print elapsed time
void AnthonysTimer::printElapsedTime(string custom_prefix, bool reset) {
	cout << custom_prefix << "T=" << getElapsedTime() << " s\n";

	if (reset) 
		setToZero();
}

// Set to 0 (restart timer)
void AnthonysTimer::setToZero() {
	tstart = clock();
	tend = -1;
	diff = -1;
}