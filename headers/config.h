#ifndef _CONFIG_H
#define _CONFIG_H

// Maximum number of libraries that can be linked against
//  since there is a "physical" limit to how many libraries can be loaded
// For most use cases, 32 is plenty sufficient
#define MAX_LIBS 32

// Maximum number of library search paths
// Enforce to both reduce the time searching
// and to push users to be more mindful of where their libraries are located
#define MAX_LIBPATHS 8

typedef struct Config {
	const char* outfile;
	char* libpath[MAX_LIBPATHS]; // The library search path
	char* libs[MAX_LIBS]; // The list of libraries to link against
} Config;

#define MAJOR_VERSION 0
#define MINOR_VERSION 1
#define PATCH_VERSION 0

#endif