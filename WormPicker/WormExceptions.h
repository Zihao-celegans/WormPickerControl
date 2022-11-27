#pragma once
#include <stdexcept>



class WormLostException : public std::exception {
	virtual const char* what() const throw() {
		return "Worm was not found";
	}
};

class EmptyWormException : public std::exception {
	virtual const char* what() const throw() {
		return "Worm is empty, expected otherwise";
	}
};