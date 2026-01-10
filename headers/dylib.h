#ifndef _DYLIB_H_
#define _DYLIB_H_

#include "aoef.h"
#include "config.h"
#include "SymbolTable.h"
#include "SectionTable.h"
#include "RelocTable.h"


struct ImportsExports {
	struct {
		AOEFFImportEnt* entries;
		uint32_t count;
		uint32_t capacity;
	} Imports;

	struct {
		AOEFFExportEnt* entries;
		uint32_t count;
		uint32_t capacity;
	} Exports;
};


void initImports(struct ImportsExports* importsExports);
void deinitImports(struct ImportsExports* importsExports);

void initExports(struct ImportsExports* importsExports);
void deinitExports(struct ImportsExports* importsExports);

void displayImports(struct ImportsExports* importsExports);
void displayExports(struct ImportsExports* importsExports);


typedef struct DynamicLibraries DynamicLibraries;

/**
 * Imports symbols from dynamic libraries into the import table. Uses the relocation table to find unresolved symbols faster.
 * @param symbTable The symbol table containing the symbols to import
 * @param relocTable The relocation table containing unresolved symbols
 * @param dyLibTable The dynamic libraries table
 * @param importsExports The structure to store imported and exported symbols.
 */
void importSymbols(SymbolTable* symbTable, RelocTable* relocTable, DynamicLibraries* dyLibTable, struct ImportsExports* importsExports);

/**
 * Exports global symbols from the symbol table into the imports/exports structure.
 * This is only done for dynamic libraries.
 * @param symbTable The symbol table to export symbols from.
 * @param importsExports The structure to store imported and exported symbols.
 */
void exportSymbols(SymbolTable* symbTable, struct ImportsExports* importsExports);



typedef struct DynamicLibraryEntry {
	char* dlName; // Name of the dynamic library
	char** dlExportSymbs; // Names of the symbols exported by the dynamic library
	uint32_t dlExportCount;
	uint32_t dlExportCapacity;
} DyLibEntry;

typedef struct DynamicLibraries {
	DyLibEntry libs[MAX_LIBS];
	uint32_t count;
} DynamicLibraries;


/**
 * Verifies that the dynamic libraries linked against exist in the supplied search path.
 * It builds a table of dynamic libraries and their exported symbols.
 * @param config The configuration containing the library search path and libraries to link against.
 * @return A table of dynamic libraries and their exported symbols.
 */
DynamicLibraries* verifyLibraries(Config* config);

void displayDynamicLibraries(DynamicLibraries* dyLibTable);



typedef struct JumpTables {
	struct {
		uint32_t* _fjt; // Function jump table
		uint32_t fjtEntryCount; // Number of entries in the function jump table
	} fjt;

	struct {
		uint32_t* _djt; // Data jump table
		uint32_t djtEntryCount; // Number of entries in the data jump table
	} djt;
} JumpTables;

/**
 * Creates the function and data jump tables based on the unresolved symbols in the relocation table.
 * Includes the dynamic relocations needed for the jump tables.
 * @param relocTable The relocation table containing unresolved symbols and output dynamic relocations.
 * @param symbTable The symbol table
 * @param sectTable The section table
 * @return 
 */
JumpTables* createJumpTables(RelocTable* relocTable, SymbolTable* symbTable, SectionTable* sectTable);



#endif