/*
 * Exception.cpp
 *
 *  Created on: Dec 8, 2015
 *      Author: nazgee
 */

#include "Exception.h"

Exception::Exception(const char* what) :
	std::runtime_error(what) {
}

Exception::Exception(const std::string& what) :
	std::runtime_error(what) {
}





