#ifndef _SYMBOL_TABLE_H_
#define _SYMBOL_TABLE_H_

#include <stdbool.h>

#include "aoef.h"


/**
 * A wrapper around the AOEFF symbol table structure and the string table.
 */
typedef struct SymbolTable {
	AOEFFSymEnt* symbols;
	uint32_t count;
	uint32_t cap;

	// For "quick" lookups on unresolved symbols that are potentially in dynamic libraries
	struct {
		uint32_t* unresolvedIndices;
		int count;
		int cap;
	} unresolved;

	struct {
		AOEFFStrTab strTab;
		uint32_t strbCount; // The number of bytes used in the string table
		uint32_t strbCap;   // The capacity of the string table in bytes

		uint32_t strCount; // The number of strings in the table
	} SymbolStringTable;
} SymbolTable;


SymbolTable* initSymbolTable();
void deinitSymbolTable(SymbolTable* symbTable);

void appendSymbol(SymbolTable* symbTable, AOEFFSymEnt symb);


int getSymbolByName(SymbolTable* symbTable, const char* name, int startIndex);

/* For the embedded string table */

/**
 * Appends a string to the symbol table's string table. Note that this maintains the null-terminator of the previous string.
 * Returns the index of the newly appended string in the string table for the symbol entry's seSymbName field.
 * @param symbTable The symbol table containing the string table
 * @param str The string to append
 * @return The index of the newly appended string in the string table
 */
uint32_t appendString(SymbolTable* symbTable, const char* str);

#endif