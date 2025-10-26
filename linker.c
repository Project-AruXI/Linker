#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "argparse.h"
#include "diagnostics.h"
#include "config.h"
#include "SymbolTable.h"
#include "dylink.h"


Config config;

static char** parseArgs(int argc, char const* argv[]) {
	// Init config with defaults
	config.outfile = "out.aru";
	config.libpath = "./libs";
	config.libs[0] = "std.adlib"; // Default standard library
	config.libs[1] = NULL;

	const char** infiles = NULL;

	bool showVersion = false;

	struct argparse_option options[] = {
		OPT_STRING('o', "output", &config.outfile, "output file name", NULL, 0, 0),
		OPT_BOOLEAN('v', "version", &showVersion, "show version and exit", NULL, 0, 0),
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
		printf("Aru Linker version 1.0.0\n");
		exit(0);
	}


	// Remaining arguments after options are input files
	if (argc - nparsed < 1) {
		fprintf(stderr, "No input file specified.\n");
		argparse_usage(&argparse);
		exit(-1);
	}

	infiles = malloc((nparsed + 1) * sizeof(char*));
	if (!infiles) {
		fprintf(stderr, "Out of memory\n");
		exit(1);
	}
	for (int i = 0; i < nparsed; ++i) {
		infiles[i] = argv[i];
	}
	infiles[nparsed] = NULL;


	return infiles;
}

int main(int argc, char const* argv[]) {
	char** infiles = parseArgs(argc, argv);

	for (int i = 0; infiles[i] != NULL; i++) {
		printf("%s\n", infiles[i]);
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
		merge();
		
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