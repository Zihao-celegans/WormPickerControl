/*
	SocketClass.cpp
	Utilities for socket communication
	Siming He & Zihao Li
	Jan 2022
*/

#include "WormSocket.h"

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

// Boost includes for socket creation
#include <boost/asio.hpp>

// Anthony's includes
#include "AnthonysTimer.h"
using namespace std;


// Socket constructor
WormSocket::WormSocket(Socket_Type s_type, std::string raw_ip_address, unsigned short port_num, int backlog_size) {
	ip_address = raw_ip_address;
	port = port_num;
	size = backlog_size;
	type = s_type;

	// Creating an endpoint designating a target server application
	ep = boost::asio::ip::tcp::endpoint(
		boost::asio::ip::address::from_string(raw_ip_address),
		port_num);

	if (type == SERVER) {
		try {
			// Creating an active socket
			sock = new boost::asio::ip::tcp::socket(ios);

			// Instantiating and opening an acceptor socket
			acceptor = new boost::asio::ip::tcp::acceptor(ios, ep.protocol());

			// Binding the acceptor socket to the server endpoint
			acceptor->bind(ep);

			// Starting to listen for incoming connection requests
			acceptor->listen(size);

		}
		catch (boost::system::system_error &e) {
			std::cout << "Error occured on C++ server side! Error code = " << e.code()
				<< ". Message: " << e.what() << std::endl;
			connection_failure = true;
		}
	}
	else if (type == CLIENT) {

		try {

			// Creating an active socket
			sock = new boost::asio::ip::tcp::socket(ios, ep.protocol());

			// Connecting a socket.
			sock->connect(ep);
			std::cout << "	opened and connected to launch socket" << std::endl;
			// At this point socket 'sock' is connected to 
			// the server application and can be used
			// to send data to or receive data from it.
		}
		catch (boost::system::system_error &e) {
			std::cout << "Error occured on C++ server side! Error code = " << e.code()
				<< ". Message: " << e.what() << std::endl;
			connection_failure = true;
		}
	}
}


WormSocket::~WormSocket() {
	cout << "Socket destructor called..." << endl;
}

void WormSocket::accept_connection() {
	if (type == SERVER) {
		try {
			// connect socket to send and receive data
			acceptor->accept(*sock);
		}
		catch (boost::system::system_error &e) {
			std::cout << "Error occured on C++ server side! Error code = " << e.code()
				<< ". Message: " << e.what() << std::endl;
			connection_failure = true;
		}
	}
}

std::string WormSocket::receive_data() {
	boost::asio::streambuf buf;
	boost::asio::read_until(*sock, buf, "\n");
	std::string data = boost::asio::buffer_cast<const char*>(buf.data());
	return data;
}

void WormSocket::send_data(const std::string& message) {
	const std::string msg = message;

	boost::asio::write(*sock, boost::asio::buffer(msg));
}

int WormSocket::terminate() {

	// Disconnecting socket 
	sock->close();

	// Free the dynamically allocated memory
	//free(&sock);
	//if (type == SERVER){ free(&acceptor); }

	return 0;
}

int WormSocket::server_initialization(std::string f_server) {
	// terminate();

	// Call python to launch the server
	// string command = "python " + f_server + "&";
	// system(command.c_str());

	return 0;
}

bool WormSocket::connection_failed() {
	return connection_failure;
}