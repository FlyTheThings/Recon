//Generic utilities
//Author: Bryan Poling
//Copyright (c) 2021 Sentek Systems, LLC. All rights reserved. 
#pragma once

//System Includes
#include <string>
#include <chrono>

//External Includes
#include "../../handycpp/Handy.hpp"
#include <opencv2/core.hpp>

//Project Includes
#include "EigenAliases.h"

inline double FractionalPart(double x) { return x - std::floor(x); }

inline double SecondsSinceT0Epoch(std::chrono::time_point<std::chrono::steady_clock> const & Timepoint) {
	auto duration = Timepoint.time_since_epoch();
	return double(duration.count()) * double(std::chrono::steady_clock::period::num) / double(std::chrono::steady_clock::period::den);
}

inline double SecondsSinceT0Epoch(void) {
	std::chrono::time_point<std::chrono::steady_clock> Now = std::chrono::steady_clock::now();
	auto duration = Now.time_since_epoch();
	return double(duration.count()) * double(std::chrono::steady_clock::period::num) / double(std::chrono::steady_clock::period::den);
}

inline double SecondsElapsed(std::chrono::time_point<std::chrono::steady_clock> const & Start, std::chrono::time_point<std::chrono::steady_clock> const & End) {
	auto duration = End - Start;
	return double(duration.count()) * double(std::chrono::steady_clock::period::num) / double(std::chrono::steady_clock::period::den);
}

inline double SecondsElapsed(std::chrono::time_point<std::chrono::steady_clock> const & Start) {
	std::chrono::time_point<std::chrono::steady_clock> Now = std::chrono::steady_clock::now();
	return SecondsElapsed(Start, Now);
}

//Advance a timepoint by a given amount of time (in seconds). Advancement has a resolution of 1 ms.
inline std::chrono::time_point<std::chrono::steady_clock> AdvanceTimepoint(std::chrono::time_point<std::chrono::steady_clock> const & T, double Seconds) {
	return (T + std::chrono::milliseconds(int64_t(std::round(1000.0*Seconds))));
}

//Make sure a filename is sane (no crazy characters or too short/long). This looks at the name only - it does not check the filesystem in any way
inline bool isFilenameReasonable(std::string Filename) {
	const std::string allowedChars("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890 (),[]:.<>'+=-_");
	if (Filename.empty() || (Filename.size() > 255U))
		return false;
	if (Filename.find_first_not_of(allowedChars) == std::string::npos)
		return true;
	else
		return false;
}

//Get the paths of all normal files in a given directory
inline std::vector<std::filesystem::path> GetNormalFilesInDirectory(std::filesystem::path const & DirPath) {
	std::vector<std::filesystem::path> files;
	if ((! std::filesystem::exists(DirPath)) || (! std::filesystem::is_directory(DirPath)))
		return files;

	std::filesystem::directory_iterator dirIter{DirPath};
	while (dirIter != std::filesystem::directory_iterator{}) {
		std::filesystem::path myPath = dirIter->path();
		if (std::filesystem::is_regular_file(myPath) && (! std::filesystem::equivalent(myPath, DirPath)))
			files.push_back(myPath);

		dirIter++;
	}
	return files;
}

//Starting at the given index, parse digits until we hit end of string or a non-digit. Return the number and update aInd in place
inline unsigned long StripNumFromFromOfString(std::string const & a, size_t & aInd) {
	size_t firstNonDigitInd = a.find_first_not_of("0123456789", aInd);
	unsigned long number;
	if (firstNonDigitInd == std::string::npos) {
		number = std::stoul(a.substr(aInd), nullptr);
		aInd = a.size();
	}
	else {
		number = std::stoul(a.substr(aInd, firstNonDigitInd - aInd), nullptr);
		aInd = firstNonDigitInd;
	}
	return number;
}

//String comparison for sorting strings in a more reasonable fashion than regular lexicographical ordering
inline bool StringNumberAwareCompare_LessThan(std::string const & a, std::string const & b) {
	size_t aInd = 0U;
	size_t bInd = 0U;
	
	//There are still characters remaining in both strings
	while ((aInd < a.size()) && (bInd <= b.size())) {
		bool aCharIsDigit = (a[aInd] >= 48) && (a[aInd] <= 57);
		bool bCharIsDigit = (b[bInd] >= 48) && (b[bInd] <= 57);
		
		if ((aCharIsDigit) && (! bCharIsDigit))
			return true;
		else if ((! aCharIsDigit) && (bCharIsDigit))
			return false;
		else if ((! aCharIsDigit) && (! bCharIsDigit)) {
			if (a[aInd] < b[bInd])
				return true;
			else if (a[aInd] > b[bInd])
				return false;
			else {
				//Move on to next character in each string
				aInd++;
				bInd++;
			}
		}
		else {
			//Both strings contain digits in the current places
			unsigned long aNum = StripNumFromFromOfString(a, aInd);
			unsigned long bNum = StripNumFromFromOfString(b, bInd);
			
			if (aNum < bNum)
				return true;
			else if (aNum > bNum)
				return false;
			//else, continue parsing
		}
	}
	
	//The empty string is "less than" the non-empty string. If both are empty, the "less than" should return false
	if ((aInd >= a.size()) && (bInd < b.size()))
		return true;
	else
		return false;
}

//remove leading and trailing characters in s matching characters in the given set
inline void StringStrip(std::string & s, std::string CharacterSet) {
	size_t startpos = s.find_first_not_of(CharacterSet);
	if (startpos == std::string::npos) s = std::string();
	else s = s.substr(startpos);
	
	size_t endpos = s.find_last_not_of(CharacterSet);
	if (endpos == std::string::npos) s = std::string();
	else s = s.substr(0, endpos+1);
}

//remove leading and trailing whitespace from string
inline void StringStrip(std::string & s)  { StringStrip (s, std::string(" \t")); }

//Break up a string into pieces by searching for delimiters. All delimiters will be removed from the string. Multiple delimiters in a row are treated as one.
inline std::vector<std::string> StringSplit(std::string const & s, std::string DelimiCharacterSet) {
	std::vector<std::string> parts;
	
	size_t pos = s.find_first_not_of(DelimiCharacterSet);
	while (pos != std::string::npos) {
		size_t nextpos = s.find_first_of(DelimiCharacterSet, pos);
		if (nextpos == std::string::npos)
			parts.push_back(s.substr(pos));
		else
			parts.push_back(s.substr(pos, nextpos - pos));
		
		StringStrip(parts.back(), DelimiCharacterSet);
		pos = s.find_first_not_of(DelimiCharacterSet, nextpos);
	}
	
	return parts;
}

//Try to convert a string to a double. Return true upon success and false on failure. Leaves value unchanged on failure.
inline bool str2double(std::string str, double & Value) {
	try {
		double convertedValue = stod(str);
		Value = convertedValue;
		return true;
	}
	catch (...) {
		return false;
	}
}

//Try to convert a string to an int. Return true upon success and false on failure. Leaves value unchanged on failure.
inline bool str2int(std::string str, int & Value) {
	try {
		int convertedValue = stoi(str);
		Value = convertedValue;
		return true;
	}
	catch (...) {
		return false;
	}
}

template<typename ValueType> ValueType getMedian(std::vector<ValueType> const & values) {
	std::vector<ValueType> valueVecLocal;
	valueVecLocal.insert(valueVecLocal.begin(), values.cbegin(), values.cend());
	return getMedian(valueVecLocal, true);
}

//Return the median of a collection of floats or doubles - return 0.0 for an empty input collection
//If the input container is a vector and you don't care if its values are re-arranged, you can set the SortInPlace flag to speed up processing
template<typename ValueType> ValueType getMedian(std::vector<ValueType> & values, bool SortInPlace) {
	std::vector<ValueType> valueVecLocal;
	std::vector<ValueType> * valueVec;
	if (SortInPlace)
		valueVec = &(values);
	else {
		valueVecLocal.insert(valueVecLocal.begin(), values.cbegin(), values.cend());
		valueVec = &(valueVecLocal);
	}
	
	//Handle special cases (size equals 0, 1, or 2)
	if (valueVec->empty()) {
		fprintf(stderr,"Warning: Can't take median of empty collection.\r\n");
		return(0.0);
	}
	else if (valueVec->size() == 1U)
		return (*valueVec)[0];
	else if (valueVec->size() == 2U)
		return (ValueType(0.5)*(*valueVec)[0] + ValueType(0.5)*(*valueVec)[1]);
	
	//Handle case that there are 3 or more elements
	if (valueVec->size() % 2U == 1U) {
		//Odd number of elements
		size_t medianIndex = (valueVec->size() - 1U) / 2U;
		std::nth_element(valueVec->begin(), valueVec->begin() + medianIndex, valueVec->end());
		return (*valueVec)[medianIndex];
	}
	else {
		//Even number of elements
		size_t medianIndex1 = (valueVec->size() - 2U) / 2U;
		size_t medianIndex2 = medianIndex1 + 1U;
		std::nth_element(valueVec->begin(), valueVec->begin() + medianIndex1, valueVec->end());
		ValueType val1 = (*valueVec)[medianIndex1];
		std::nth_element(valueVec->begin(), valueVec->begin() + medianIndex2, valueVec->end());
		ValueType val2 = (*valueVec)[medianIndex2];
		return (ValueType(0.5)*val1 + ValueType(0.5)*val2);
	}
}

cv::Mat GetRefFrame(std::filesystem::path const & DatasetPath);

//Return the path of the first .MOV file in the given folder (lexicographically)
std::filesystem::path GetSimVideoFilePath(std::filesystem::path const & DatasetPath);

//Load GCPs from the file GCP.txt in the given folder - we also adjust them from file native res (4K) to 720p
//Returned fiducials are in form needed by Shadow Detection Engine.
std::Evector<std::tuple<Eigen::Vector2d, Eigen::Vector3d>> LoadFiducialsFromFile(std::filesystem::path const & DatasetPath);



