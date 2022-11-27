/*
 	AnthonysTimer.h
 	Anthony Fouad
 	Fang-Yen Lab
	Updated with Class implementation of timer May 2018
	Deleted old #define implementation


	USAGE:


	AnthonysTimer Ta();		Start running a timer. 
							Use Ta.getElapsedTime() to find out how much time has passed
							Use Ta.setToZero() to restart timer

	AnthonysTimer Tb(expr,sec) 
							to start running a timer and block execution until expr is FALSE
							OR sec expires.

							Alternative using Ta:
							Ta.setToZero();
							Ta.blockForTime();
 		
 */

#pragma once

#ifndef ANTHONYSTIMER_H
#define ANTHONYSTIMER_H


#include "time.h"
#include <string>


class AnthonysTimer {

public:

	// Constructor (starts the timer and optionally blocks execution if seconds is > 0 - see blockForTime)
	AnthonysTimer();
	AnthonysTimer(bool test_expression, double seconds);

	// Simple implementation of a blocker function. Blocks while test_expr is true AND time is less than seconds.
	void blockForTime(bool test_expression, double seconds, double sleep_interval_ms = 25);

	// Update time difference - can be used for custom implementations of a blocker. Also returns elapsed time
	double getElapsedTime();
	double getElapsedTimeThreadSafe() const; /// Read only version that can be used in threads

	void printElapsedTime(std::string custom_prefix = "",bool reset = false);

	// Set timer to 0 (restart the timer, similar to constructor)
	void setToZero();

protected:
	time_t tstart, tend;
	double diff;
};

#endif