/*
 *		SimpleSerial.h
 *		adapted by Anthony Fouad
 *		Fang-Yen Group, 6/2014
 *
 *		Class for the serial port connection to the arduino via BOOST libaries.
 *
 *		Adapted from 2 sites:
 *		https://groups.google.com/forum/#!topic/boostusers/0RpVAVSTQXQ
 *		http://www.webalice.it/fede.tft/serial_port/serial_port.html
 */

#ifndef SIMPLESERIAL_H_
#define SIMPLESERIAL_H_

#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <string>
using namespace std;

typedef boost::asio::serial_port_base asio_serial;

class SimpleSerial {

private:
	boost::asio::io_service io;
	boost::asio::serial_port serial;

public:

	// Serial port constructor 
    SimpleSerial(string port, unsigned int baud_rate);

	// Write data to the serial port
    void writeString(string s);

	// Read a line of data from the serial port
	string readLine();

	// Read all available lines of data from the serial port
	void readAllLines();

	// Set servo angle
	void writeServoAngle();

	// Pick position information
	int currentpos, newpos, uppos, downpos, posLimitUp, posLimitDown;
};

#endif