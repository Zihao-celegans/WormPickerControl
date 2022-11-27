/*
	SocketProgramming.cpp
	Zihao Li
	Fang-Yen Lab, Dec 2021
	Utility functions for socket programming

*/


#include "SocketProgramming.h"

// Standard includes
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

// Anthony's includes
#include "AnthonysTimer.h"

using namespace std;

//void handler(
//	const boost::system::error_code& error, // Result of operation.
//
//	std::size_t bytes_transferred           // Number of bytes written from the
//											// buffers. If an error occurred,
//											// this will be less than the sum
//											// of the buffer sizes.
//) {}
//
//
//string socketRead(boost::asio::ip::tcp::socket & socket) {
//	boost::asio::streambuf buf;
//	boost::asio::read_until(socket, buf, "\n");
//	string data = boost::asio::buffer_cast<const char*>(buf.data());
//	return data;
//}
//
//void socketSend(boost::asio::ip::tcp::socket& socket, const string& message) {
//	const string msg = message + "\n";
//
//	boost::asio::write(socket, boost::asio::buffer(message));
//
//	/*
//		async_write vs write: Async returns immediately but ccould trigger ahandle if finished or error.
//								Write, meanwhile, blocks until finished.
//
//		For the moment, just async write sleep for 200 ms
//	*/
//	//boost::asio::async_write(socket, boost::asio::buffer(message), handler);
//	//boost_sleep_ms(2000);
//}
//
//
///*
//
//	Server start / terminate
//*/
//
//// Send the terminate signal to the server
//int serverTerminate(std::string raw_ip_address, unsigned short port_num) {
//
//	boost::asio::ip::tcp::endpoint
//		ep(boost::asio::ip::address::from_string(raw_ip_address), port_num);
//	boost::asio::io_service ios;
//	boost::asio::ip::tcp::socket sock(ios, ep.protocol());
//
//	try {
//
//		// Connect to server, if it exists (see maskRCNNremoteinference forcomments)
//		sock.connect(ep);
//
//		// write the terminate signal to the socket (should disconnect within 10 seconds)
//		socketSend(sock, "terminate");
//	}
//	catch (int e) {
//		sock.terminate();
//		return -1;
//	} // If failed to connect, just return
//
//	// Wait for server to ackgnowledge, up to max wait time
//	string response = "";
//	AnthonysTimer Twait;
//	double Tmax = 0.75;
//	while (Twait.getElapsedTime() < Tmax && response.size() == 0) {
//		response = socketRead(sock);
//	}
//	if (Twait.getElapsedTime() > Tmax)
//		std::cout << "ERROR: Failed to receive confirmation of termination from PyTorch server!" << endl;
//
//	// Step 8. Disconnecting socket 
//	sock.terminate();
//
//	return 0;
//}
//
//int serverInitialize(string f_server) {
//
//	// First, attempt to terminate the server in case it is already running
//	serverTerminate();
//
//	// Call python to launch the server
//	string command = "python " + f_server + "&";
//	system(command.c_str());
//
//	return 0;
//
//}