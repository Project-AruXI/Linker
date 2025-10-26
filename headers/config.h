#ifndef _CONFIG_H
#define _CONFIG_H

// Maximum number of libraries that can be linked against
//  since there is a "physical" limit to how many libraries can be loaded
// For most use cases, 32 is plenty sufficient
#define MAX_LIBS 32

typedef struct Config {
	const char* outfile;
	const char** libpath; // The Null-terminated library search path
	char* libs[MAX_LIBS]; // The list of libraries to link against
} Config;

#endif