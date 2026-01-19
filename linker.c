#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "argparse.h"
#include "diagnostics.h"
#include "config.h"
#include "dylib.h"
#include "merge.h"
#include "relocate.h"
#include "binwriter.h"


Config config = {
	.outfile = "out.aru",
	.noStdLib = false,
	.isKernel = false,
	.isDynamic = false,
	.libpath = { "./libs", NULL },
	.libs    = { "std", NULL },
};


static int linkLibCallback(struct argparse* self, const struct argparse_option* option) {
	if (!self->optvalue) {
		emitWarning(WARN_INVALID, "No library name specified.");
		return 0;
	}

	// Each time this is called, a new library is being added
	// Find the first NULL entry in config.libs and add it there
	for (int i = 1; i < MAX_LIBS; i++) {
		if (config.libs[i] == NULL) {
			// In the case the linked library ends with .adlib, take it out
			// If it has a suffix other than .adlib, issue a warning and ignore
			// If it has no suffix, assume it's correct
			size_t len = strlen(self->optvalue);
			if (len > 7 && strcmp(&self->optvalue[len - 7], ".adlib") == 0) {
				// Valid .adlib suffix, remove it
				((char*)self->optvalue)[len - 7] = '\0';
			} else if (len > 4) {
				// Has a suffix, but not .adlib
				emitWarning(WARN_INVALID, "Linked library %s has an unrecognized suffix, expected .adlib. Ignoring suffix.", self->optvalue);
			} else if (strcmp(self->optvalue, ".adlib") == 0) {
				// Only suffix, no library name
				emitWarning(WARN_INVALID, "Linked library name is missing before .adlib suffix. Ignoring.");
			} // Else, no suffix, assume it's correct

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
		OPT_BOOLEAN('k', "kernel", &config.isKernel, "build as a kernel binary", NULL, 0, 0),
		OPT_BOOLEAN('d', "dynamic", &config.isDynamic, "build as a dynamic library", NULL, 0, 0),
		OPT_STRING(0, "library", NULL, "library to link against", &linkLibCallback, 0, 0),
		OPT_STRING(0, "libpath", NULL, "library search path", &libpathCallback, 0, 0),
		OPT_BOOLEAN(0, "no-stdlib", &config.noStdLib, "do not link against the standard library", NULL, 0, 0),
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

	// Check flag compatibility
	if (config.isKernel && config.isDynamic) {
		emitError(ERR_OTHER, "Cannot build a kernel binary as a dynamic library.");
	}

	// Remaining arguments after options are input files
	if (nparsed == 0) {
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
	DynamicLibraries* dyLibTable = verifyLibraries(&config);
	displayDynamicLibraries(dyLibTable);

	uint32_t dataStart = 0x0;
	uint32_t constStart = 0x0;
	uint32_t bssStart = 0x0;
	uint32_t textStart = 0x0;

	if (config.isKernel) {
		dataStart = 0xA0080000;
		textStart = 0xB8080000;
	} else if (!config.isKernel && !config.isDynamic) {
		// Not kernel and not dynamic means its a normal executable
		dataStart = 0x20090000;
		constStart = 0x20080000;
		bssStart = 0x20040000;
		textStart = 0x20190000;
	} else if (config.isDynamic) {
		// There is no "starting" for dynamic libraries, they are loaded at arbitrary locations
	}

	GlobalTables globalTables;
	globalTables.symbolTable = initSymbolTable();
	globalTables.sectionTable = initSectionTable(dataStart, constStart, bssStart, textStart);
	globalTables.relocTable = initRelocTable();

	for (int i = 0; infiles[i] != NULL; i++) {
		const char* infile = infiles[i];
		rlog("Merging input file: %s", infile);
		merge(infile, &globalTables);
	}

	if (globalTables.sectionTable->sections[4].shSectSize != 0 && !config.isKernel) {
		emitError(ERR_INVALID_FORMAT, "Evt section found in non-kernel binary.");
	}

	displayRelocTable(globalTables.relocTable);
	displaySymbolTable(globalTables.symbolTable);
	displaySectionTable(globalTables.sectionTable);

	// At this point, symbol tables have been merged, section tables have been merged,
	//   relocation tables have been merged, and contents (text and data) have been merged
	// Relocations are now to be made

	relocate(globalTables.relocTable, globalTables.sectionTable, globalTables.symbolTable);

	struct ImportsExports importsExports;
	// Any remaining unresolved symbols/relocations are to be from dynamic libraries
	// Verify that is true (check all leftover symbols are found in the linked libraries)
	initImports(&importsExports);
	importSymbols(globalTables.symbolTable, globalTables.relocTable, dyLibTable, &importsExports);

	JumpTables* jumpTables = createJumpTables(globalTables.relocTable, globalTables.symbolTable, globalTables.sectionTable);

	// globalTables.symbolTable = dropLocalSymbols(globalTables.symbolTable);
	// globalTables.relocTable = dropStaticReloc(globalTables.relocTable, globalTables.symbolTable);

	initExports(&importsExports);
	if (config.isDynamic) exportSymbols(globalTables.symbolTable, &importsExports);


	writeBinary(&config, &globalTables, &importsExports, jumpTables, dyLibTable);

	displayRelocTable(globalTables.relocTable);


	// deinitSymbolTable(globalSymTable);
	free(infiles);

	return 0;
}