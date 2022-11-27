/*
	MatIndex.h
	Anthony Fouad
	Fang-Yen Lab
	Fall 2018

	Defines a class, similar to a cv::Point, for storing a matrix element's coordinates in row,col format
*/


#pragma once
#ifndef MATINDEX_H
#define MATINDEX_H

#include <type_traits>
#include <string>

class MatIndex {

public:

	// Constructors
	MatIndex() { row = 0; col = 0; }
	MatIndex(int r, int c) : row(r), col(c){}

	bool MatIndex::operator==(const MatIndex& rhs) const {
		return row == rhs.row && col == rhs.col;
	}

	// Data
	int row = 0, col = 0;

};

//std::ostream& operator<< (std::ostream &out, MatIndex const& mat_index) {
//	out << "(" << mat_index.row << ", " << mat_index.col << ")";
//	return out;
//}

namespace std
{
	template<> struct std::hash<MatIndex>
	{
		std::size_t operator()(MatIndex const& idx) const noexcept
		{
			std::size_t h1 = std::hash<std::string>{}(std::to_string(idx.row));
			std::size_t h2 = std::hash<std::string>{}(std::to_string(idx.col));
			return h1 ^ (h2 << 1);
		};
	};
};


//struct MatIndexHash
//{
//	std::size_t operator()(MatIndex const& idx) const noexcept
//	{
//		std::size_t h1 = std::hash<std::string>{}(std::to_string(idx.row));
//		std::size_t h2 = std::hash<std::string>{}(std::to_string(idx.col));
//		return h1 ^ (h2 << 1);
//	};
//};

#endif