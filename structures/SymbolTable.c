#include <stdlib.h>
#include <string.h>

#include "SymbolTable.h"
#include "diagnostics.h"


SymbolTable* initSymbolTable() {
	SymbolTable* symbTable = (SymbolTable*) malloc(sizeof(SymbolTable));
	if (!symbTable) emitError(ERR_MEM, "Failed to allocate memory for symbol table");

	symbTable->symbols = (AOEFFSymbEntry*) malloc(10 * sizeof(AOEFFSymbEntry));
	if (!symbTable->symbols) emitError(ERR_MEM, "Failed to allocate memory for symbol table entries");

	symbTable->count = 0;
	symbTable->cap = 10;


	uint32_t* unresolvedIndices = (uint32_t*) malloc(10 * sizeof(uint32_t));
	if (!unresolvedIndices) emitError(ERR_MEM, "Failed to allocate memory for unresolved symbol indices");

	symbTable->unresolved.unresolvedIndices = unresolvedIndices;
	symbTable->unresolved.count = 0;
	symbTable->unresolved.cap = 10;


	char* strTabData = (char*) malloc(50 * sizeof(char));
	if (!strTabData) emitError(ERR_MEM, "Failed to allocate memory for symbol string table");

	symbTable->SymbolStringTable.strTab.stStrs = strTabData;
	symbTable->SymbolStringTable.strbCount = 0;
	symbTable->SymbolStringTable.strbCap= 50;
	symbTable->SymbolStringTable.strCount = 0;

	return symbTable;
}

void deinitSymbolTable(SymbolTable* symbTable) {
	free(symbTable->symbols);
	free(symbTable->SymbolStringTable.strTab.stStrs);
	free(symbTable->unresolved.unresolvedIndices);
	free(symbTable);
}

void appendSymbol(SymbolTable* symbTable, AOEFFSymbEntry symb) {
	if (symbTable->count >= symbTable->cap) {
		symbTable->cap *= 2;
		symbTable->symbols = (AOEFFSymbEntry*) realloc(symbTable->symbols, symbTable->cap * sizeof(AOEFFSymbEntry));
		if (!symbTable->symbols) emitError(ERR_MEM, "Failed to reallocate memory for symbol table entries");
	}
	symbTable->symbols[symbTable->count++] = symb;
}

void appendString(SymbolTable* symbTable, const char* str) {
	char* strs = symbTable->SymbolStringTable.strTab.stStrs;

	size_t len = strlen(str) + 1; // +1 for null terminator
	if (symbTable->SymbolStringTable.strbCount + len == symbTable->SymbolStringTable.strbCap) {
		symbTable->SymbolStringTable.strbCap *= 2;

		strs = (char*) realloc(strs, symbTable->SymbolStringTable.strbCap * sizeof(char));
		if (!strs) emitError(ERR_MEM, "Failed to reallocate memory for symbol string table");
		symbTable->SymbolStringTable.strTab.stStrs = strs;
	}

	char* dest = &strs[symbTable->SymbolStringTable.strbCount];
	// dest should be the null terminator of the previous string
	if (*dest != '\0') emitError(ERR_INTERNAL, "Symbol string table is corrupted (missing null terminator)");
	dest++; // Begin copying after null
	strcpy(dest, str);
	symbTable->SymbolStringTable.strbCount += len;
	symbTable->SymbolStringTable.strCount += 1;
}