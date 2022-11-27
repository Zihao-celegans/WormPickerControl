/*
 *		SimpleSerial.cpp
 *		adapted by Anthony Fouad
 *		Fang-Yen Group, 6/2014
 *
 *		Class for the serial port connection to the stage.
 *
 *		Adapted from 2 sites:
 *		https://groups.google.com/forum/#!topic/boostusers/0RpVAVSTQXQ
 *		http://www.webalice.it/fede.tft/serial_port/serial_port.html
 *
 *		All source code from webalice.it:
 *		https://github.com/fedetft/serial-port
 */

#include "SimpleSerial.h"
#include <iostream>
typedef boost::asio::serial_port_base asio_serial;


/*
* Constructor.
*		\param port device name, example "COM4"
*		\param baud_rate communication speed, example 9600 or 115200
*		\throws boost::system::system_error if cannot open the serial device
*/

SimpleSerial::SimpleSerial(string port, unsigned int baud_rate):io(),serial(io,port){	

	// Setup serial connection
		serial.set_option(asio_serial::baud_rate(baud_rate));
		serial.set_option(asio_serial::flow_control(asio_serial::flow_control::none ) );
		serial.set_option(asio_serial::parity(asio_serial::parity::none ) );
		serial.set_option(asio_serial::stop_bits( asio_serial::stop_bits::one ) );
		serial.set_option(asio_serial::character_size( 8 ) );

	// Set default limits
		currentpos = 100;
		newpos = 100;
		posLimitDown = 110;
		posLimitUp = 90;
		uppos = 100;
		downpos = 105;

}


/*
 * Write a string to the serial device.
 *		\param s string to write
 *		\throws boost::system::system_error on failure
 */

void SimpleSerial::writeString(string s){
	//boost::this_thread::sleep(boost::posix_time::milliseconds(100));
	boost::asio::write(serial,boost::asio::buffer(s.c_str(),s.size()));
}

/*
	* Read a line, maximum length 1000 characters
	* Eventual '\n' or '\r\n' characters at the end of the string are removed.
	* \return a string containing the received line
	* \throws boost::system::system_error on failure
*/
std::string SimpleSerial::readLine()
{
	//Reading data char by char, code is optimized for simplicity, not speed
	using namespace boost;
	char c;
	std::string result;
	for (;;)
	{
		asio::read(serial, asio::buffer(&c, 1));
		switch (c)
		{
		case '\r':
			break;
		case '\n':
			return result;
		default:
			result += c;
		}
	}
}

/*
	Read all available lines (maximum 100)
*/

void SimpleSerial::readAllLines() {

	// Allocate space for an empty string;
	string *s = new string;
	int emptycount = 0;
	// Print strings until 5 empties in a row are retrieved
	while (emptycount < 5) {

		// Read the line
		*s = readLine();

		// If line, print it and clear emptycount. If not, increment emptycount
		if (s->size() == 0) { emptycount++; }
		else {
			cout << *s << endl;
			emptycount = 0;
		}

	}
}

/*
 * Specify an angle to move the servo to. Validate angle range first.
 */

void SimpleSerial::writeServoAngle() {

	if (newpos <= posLimitDown && newpos >= posLimitUp && newpos != currentpos) {
		std::string s = std::to_string(newpos);
		s.append("\n");
		writeString(s);
		//cout << s << endl;
		currentpos = newpos;
	}
}
