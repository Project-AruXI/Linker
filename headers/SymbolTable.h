#ifndef _SYMBOL_TABLE_H_
#define _SYMBOL_TABLE_H_

#include <stdbool.h>

#include "aoef.h"


/**
 * A wrapper around the AOEFF symbol table structure and the string table.
 */
typedef struct SymbolTable {
	AOEFFSymEnt* symbols;
	int* filectxIndices; // Indices of the file contexts each symbol belongs to
	uint32_t count;
	uint32_t cap;

	struct {
		AOEFFStrTab strTab;
		uint32_t strbCount; // The number of bytes used in the string table
		uint32_t strbCap;   // The capacity of the string table in bytes

		uint32_t strCount; // The number of strings in the table
	} SymbolStringTable;
} SymbolTable;


SymbolTable* initSymbolTable();
void deinitSymbolTable(SymbolTable* symbTable);

AOEFFSymEnt* appendSymbol(SymbolTable* symbTable, AOEFFSymEnt symb);


int getSymbolByName(SymbolTable* symbTable, const char* name, int startIndex);

void displaySymbolTable(SymbolTable* symbTable);

/* For the embedded string table */

/**
 * Appends a string to the symbol table's string table. Note that this maintains the null-terminator of the previous string.
 * Returns the index of the newly appended string in the string table for the symbol entry's seSymbName field.
 * @param symbTable The symbol table containing the string table
 * @param str The string to append
 * @return The index of the newly appended string in the string table
 */
uint32_t appendString(SymbolTable* symbTable, const char* str);


/**
 * Removes all local symbols from the symbol table. This requires readjustment of the symbol table and the string table.
 * Thus it returns a new symbol table instance with only global symbols.
 * @param symbTable The symbol table to drop local symbols from
 * @return The new symbol table instance with only global symbols
 */
SymbolTable* dropLocalSymbols(SymbolTable* symbTable);

#endif