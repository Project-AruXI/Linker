#ifndef _DYLIB_TABLE_H_
#define _DYLIB_TABLE_H_

#include "aoef.h"


typedef struct DyLibTableEntry {
	char* lib; // The name of the dynamic library
	AOEFFSymbEntry* symbols; // The symbols used from this library
	// There is the need to store the symbol names since the AOEFFSymbEntry only stores an index into the string table
	//  and the string table of the dynamic library is not stored here
	// This is mainly for diagnostics and error reporting
	char** symbNames; // The names of the symbols used from this library
	uint32_t symbCount; // The number of symbols used from this library
	uint32_t symbCap; // The capacity of the symbols array
} DyLibTableEntry;

typedef struct DyLibTable {
	DyLibTableEntry* entries;
	uint32_t count;
	uint32_t cap;
} DyLibTable;


DyLibTable* initDyLibTable();
void deinitDyLibTable(DyLibTable* table);

/**
 * Adds a symbol to the dynamic library table under the specified library name, adding the library if it does not already exist.
 * Note that the symbol is stored merely as a reference, meaning the table does not take ownership.
 * @param dylibTable The dynamic library table
 * @param libName The name of the library
 * @param symbol The symbol to add
 * @param symbName The name of the symbol to add
 * @return The dynamic library table entry the symbol was added to
 */
DyLibTableEntry* addDyLibSymb(DyLibTable* dylibTable, const char* libName, AOEFFSymbEntry* symbol, const char* symbName);

void showDyLibTable(DyLibTable* table);


#endif