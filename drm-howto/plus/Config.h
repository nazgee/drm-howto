/*
 * Config.h
 *
 *  Created on: Dec 8, 2015
 *      Author: nazgee
 */

#ifndef PLUS_CONFIG_H_
#define PLUS_CONFIG_H_

namespace Config {
	void parse(int argc, char **argv);
	const char* getDrmNodeName();
};

#endif /* PLUS_CONFIG_H_ */
