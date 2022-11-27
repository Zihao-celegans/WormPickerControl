/*
	SocketClass.h
	Utilities for socket communication
	Siming He & Zihao Li
	Jan 2022
*/

#pragma once
#ifndef WormSocket_H
#define WormSocket_H

#include <string>

// Boost includes for socket creation
#include <boost/asio.hpp>

enum Socket_Type {SERVER, CLIENT};

class WormSocket {
private:
	std::string ip_address;
	unsigned short port;
	boost::asio::ip::tcp::endpoint ep;
	boost::asio::io_service ios;
	boost::asio::ip::tcp::socket *sock = (boost::asio::ip::tcp::socket*) calloc(1, sizeof(boost::asio::ip::tcp::socket));
	boost::asio::ip::tcp::acceptor *acceptor = (boost::asio::ip::tcp::acceptor*) calloc(1, sizeof(boost::asio::ip::tcp::acceptor));
	int size; // The size of the queue containing the pending client connection requests. (usually only useful for server socket)
	Socket_Type type;
	bool connection_failure = false;

public:
	WormSocket(Socket_Type s_type, std::string raw_ip_address, unsigned short port_num, int backlog_size = 30);
	~WormSocket();

	void accept_connection();

	std::string receive_data();

	void send_data(const std::string& message);

	int terminate();

	int server_initialization(std::string f_server);

	bool connection_failed();
};

#endif