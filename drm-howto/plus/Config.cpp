/*
 * Config.cpp
 *
 *  Created on: Dec 8, 2015
 *      Author: nazgee
 */

#include <iostream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdexcept>
#include <errno.h>
#include <stdlib.h>
#include "Config.h"

static const char* sDrmDeviceName = "foo";

void Config::parse(int argc, char **argv)
{
	int c;
	while (1) {
		static struct option long_options[] = {
				{ "device", required_argument, NULL, 'd' },
//				{ "manual", no_argument, NULL, 'm' },
//				{ "server", required_argument, NULL, 's' },
				{ 0, 0, 0, 0 }
			};

		/* getopt_long stores the option index here. */
		int option_index = 0;

		c = getopt_long(argc, argv, "d:", long_options, &option_index);

		/* Detect the end of the options. */
		if (c == -1)
			break;

		switch (c) {
			case 0: {
				/* If this option set a flag, do nothing else now. */
				if (long_options[option_index].flag != 0)
					break;
				std::cout << "option " << long_options[option_index].name;
				if (optarg)
					std::cout << " with arg " << optarg;
				std::cout << std::endl;
			} break;
			case 'd': {
				sDrmDeviceName = optarg;
			} break;
			case '?': {
				/* getopt_long already printed an error message. */
				throw std::invalid_argument("Bad params given!");
			}break;

			default:
				abort();
				break;
		}
	}

	/* Print any remaining command line arguments (not options). */
	if (optind < argc) {
		std::cerr << "non-option ARGV-elements: ";
		while (optind < argc)
			std::cerr << argv[optind++];
		std::cerr << std::endl;
	}
}

const char* Config::getDrmNodeName()
{
	return sDrmDeviceName;
}
