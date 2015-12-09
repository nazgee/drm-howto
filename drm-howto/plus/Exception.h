/*
 * Exception.h
 *
 *  Created on: Dec 8, 2015
 *      Author: nazgee
 */

#ifndef PLUS_EXCEPTION_H_
#define PLUS_EXCEPTION_H_

#include <stdexcept>
#include <string>

class Exception : public std::runtime_error {
public:
	Exception(const char* what);
	Exception(const std::string& what);
	virtual ~Exception() throw() {};
};

#endif /* PLUS_EXCEPTION_H_ */
