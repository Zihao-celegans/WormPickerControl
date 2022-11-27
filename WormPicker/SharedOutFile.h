/*
	SharedOutFile.h
	Anthony Fouad
	Fang-Yen Lab
	Fall 2018

	Defines the SharedOutFile class, which is a container for an ofstream that can be locked using
	boost to prevent other threads from accessing the ofstream. 

	ofstream is actually thread-safe by itself, but if one thread closes the stream while another tries 
	to write to it, a crash may occur
*/
#pragma once
#ifndef SHAREDOUTFILE_H
#define SHAREDOUTFILE_H


// Boost includes
#include "boost\thread.hpp"
#include "boost\thread\lockable_adapter.hpp"

// Standard includes
#include <fstream>
#include <string>

class SharedOutFile : public boost::basic_lockable_adapter<boost::mutex>
{
public:
	
	// Constructor opens the outfile
	SharedOutFile(std::string fname, int mode) {
		stream.open(fname,mode);
	}
	SharedOutFile(std::string fname) {
		stream.open(fname);
	}
	SharedOutFile() {}


	// Destructor closes the outfile
	~SharedOutFile() {
		stream.close();
	}
	
	// Public data members
	std::ofstream stream;

};

#endif
