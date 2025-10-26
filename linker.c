#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "argparse.h"
#include "diagnostics.h"
#include "config.h"
#include "SymbolTable.h"
#include "dylink.h"


Config config = {
		.outfile = "out.aru",
		.libpath = { "./libs", NULL },
		.libs    = { "std.adlib", NULL },
};


static int linkLibCallback(struct argparse* self, const struct argparse_option* option) {
	// Each time this is called, a new library is being added
	// Find the first NULL entry in config.libs and add it there
	for (int i = 1; i < MAX_LIBS; i++) {
		if (config.libs[i] == NULL) {
			config.libs[i] = (char*) self->optvalue;
			config.libs[i + 1] = NULL;
			break;
		}
	}

	return 0;
}

static int libpathCallback(struct argparse* self, const struct argparse_option* option) {
	// Each time this is called, a new library path is being added
	// Find the first NULL entry in config.libpath and add it there
	for (int i = 1; i < MAX_LIBPATHS; i++) {
		if (config.libpath[i] == NULL) {
			config.libpath[i] = (char*) self->optvalue;
			config.libpath[i + 1] = NULL;
			break;
		}
	}

	return 0;
}

static const char** parseArgs(int argc, char const* argv[]) {
	const char** infiles = NULL;

	bool showVersion = false;

	struct argparse_option options[] = {
		OPT_STRING('o', "output", &config.outfile, "output file name", NULL, 0, 0),
		OPT_BOOLEAN('v', "version", &showVersion, "show version and exit", NULL, 0, 0),
		OPT_STRING('l', "library", NULL, "library to link against", &linkLibCallback, 0, 0),
		OPT_STRING('L', "libpath", NULL, "library search path", &libpathCallback, 0, 0),
		OPT_HELP(),
		OPT_END(),
	};

	const char* const usages[] = {
		"arxlnk [options] ...files",
		NULL
	};

	struct argparse argparse;
	argparse_init(&argparse, options, usages, 0);
	argparse_describe(&argparse, "Aru Linker", NULL);
	int nparsed = argparse_parse(&argparse, argc, argv);

	if (showVersion) {
		printf("Aru Linker version %d.%d.%d\n", MAJOR_VERSION, MINOR_VERSION, PATCH_VERSION);
		exit(0);
	}


	// Remaining arguments after options are input files
	if (argc - nparsed < 1) {
		fprintf(stderr, "No input file specified.\n");
		argparse_usage(&argparse);
		exit(-1);
	}

	infiles = malloc((nparsed + 1) * sizeof(char*));
	if (!infiles) emitError(ERR_MEM, "Failed to allocate memory for input file list");

	for (int i = 0; i < nparsed; ++i) {
		infiles[i] = argv[i];
	}
	infiles[nparsed] = NULL;


	return infiles;
}

int main(int argc, char const* argv[]) {
	initScope("main");

	const char** infiles = parseArgs(argc, argv);

	rlog("Output file: %s", config.outfile);
	rlog("Input files:");
	for (int i = 0; infiles[i] != NULL; i++) {
		rlog("%s", infiles[i]);
	}

	for (int i = 0; config.libs[i] != NULL; i++) {
		rlog("Linking against library: %s", config.libs[i]);
	}

	for (int i = 0; config.libpath[i] != NULL; i++) {
		rlog("Library search path: %s", config.libpath[i]);
	}

	// Validate that the linked libraries exists
	// ...

	// Plan to combine files:
	// Have a global everything (global symbol table, global string table, global sections)
	// Go through each input file
	//   For each input file, read its sections and symbols
	//   Add its components to the global ones, adjust addresses as necessary
	//     Resolve relocations as necessary
	//     Ensure no symbol conflicts
	// Look up any library dependencies from the input files and check symbols
	// Write final binary, injecting initialization code

	SymbolTable* globalSymTable = initSymbolTable();



	for (int i = 0; infiles[i] != NULL; i++) {
		const char* infile = infiles[i];
		// merge();
		
	}

	// All unresolved symbols at this point should be unresolved due to them being in dynamic libraries
	// The dynamic library table will hold the libraries actually used and the symbols that "imported"

	DyLibTable* dyLibTable = dynLibBuild(globalSymTable, config.libpath, config.libs);
	showDyLibTable(dyLibTable);
	 




	deinitSymbolTable(globalSymTable);
	deinitDyLibTable(dyLibTable);
	free(infiles);

	return 0;
}