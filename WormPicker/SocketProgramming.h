#pragma once
#ifndef SOCKETPROGRAMMING_H
#define SOCKETPROGRAMMING_H

#include <string>

// Boost includes for socket creation
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/algorithm/string.hpp>
#include <regex>

/*
	Socket client adapted from
	https://subscription.packtpub.com/book/application_development/9781783986545/1/ch01lvl1sec15/connecting-a-socket

	Use it to send an image and get the masks back from PyTorch
*/


//// Handler is used if using async_write instead of write
//void handler(
//	const boost::system::error_code& error, // Result of operation.
//
//	std::size_t bytes_transferred           // Number of bytes written from the
//											// buffers. If an error occurred,
//											// this will be less than the sum
//											// of the buffer sizes.
//);
//
//
//// Read from socket
//std::string socketRead(boost::asio::ip::tcp::socket & socket);
//
//// Write to socket
//void socketSend(boost::asio::ip::tcp::socket & socket, const std::string& message);
//
//// Terminate server
//int serverTerminate(std::string raw_ip_addressess = "127.0.0.1", unsigned short port_num = 10000);
//
//// Initialize server
//int serverInitialize(std::string f_server = "C:\\WormPicker\\NeuralNets\\RCNN_Segment_Image.py");


#endif
