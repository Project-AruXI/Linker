#include <stdlib.h>
#include <string.h>

#include "SymbolTable.h"
#include "diagnostics.h"


SymbolTable* initSymbolTable() {
	SymbolTable* symbTable = (SymbolTable*) malloc(sizeof(SymbolTable));
	if (!symbTable) emitError(ERR_MEM, "Failed to allocate memory for symbol table");

	symbTable->symbols = (AOEFFSymEnt*) malloc(sizeof(AOEFFSymEnt) * 10);
	if (!symbTable->symbols) emitError(ERR_MEM, "Failed to allocate memory for symbol table entries");

	symbTable->count = 0;
	symbTable->cap = 10;

	symbTable->filectxIndices = (int*) malloc(sizeof(int) * 10);
	if (!symbTable->filectxIndices) emitError(ERR_MEM, "Failed to allocate memory for symbol table file context indices");


	uint32_t* unresolvedIndices = (uint32_t*) malloc(sizeof(uint32_t) * 10);
	if (!unresolvedIndices) emitError(ERR_MEM, "Failed to allocate memory for unresolved symbol indices");

	symbTable->unresolved.unresolvedIndices = unresolvedIndices;
	symbTable->unresolved.count = 0;
	symbTable->unresolved.cap = 10;


	char* strTabData = (char*) malloc(sizeof(char) * 50);
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

AOEFFSymEnt* appendSymbol(SymbolTable* symbTable, AOEFFSymEnt symb) {
	if (symbTable->count >= symbTable->cap) {
		symbTable->cap *= 2;
		symbTable->symbols = (AOEFFSymEnt*) realloc(symbTable->symbols, sizeof(AOEFFSymEnt) * symbTable->cap);
		if (!symbTable->symbols) emitError(ERR_MEM, "Failed to reallocate memory for symbol table entries");

		symbTable->filectxIndices = (int*) realloc(symbTable->filectxIndices, sizeof(int) * symbTable->cap);
		if (!symbTable->filectxIndices) emitError(ERR_MEM, "Failed to reallocate memory for symbol table file context indices");
	}
	symbTable->symbols[symbTable->count++] = symb;

	return &symbTable->symbols[symbTable->count - 1];
}

int getSymbolByName(SymbolTable* symbTable, const char* name, int startIndex) {
	for (uint32_t i = startIndex; i < symbTable->count; i++) {
		AOEFFSymEnt* symbEnt = &symbTable->symbols[i];
		char* symbName = &symbTable->SymbolStringTable.strTab.stStrs[symbEnt->seSymbName];

		if (strcmp(symbName, name) == 0) {
			return i;
		}
	}

	return -1;
}

void displaySymbolTable(SymbolTable* symbTable) {
	/**
	 * Display as:
	 * ===== Symbol Table =====
	 * Total symbols: x
	 * -----------------------
	 * # | Name Index | Name | Size | Value | Info (Type,Locality) | Section
	 * ---------------------------------------------------------------------
	 * x |		 x      | xxx  | 0xX  | 0xX   |   (X, X)           |   X
	 * ---------------------------------------------------------------------
	 * ...
	 */

	char* typeStr = NULL;
	char* locStr = NULL;

	rtrace("====== Symbol Table ======");
	rtrace("Total symbols: %d", symbTable->count);
	rtrace("--------------------------");
	rtrace(" # | Name Index |    Name    |  Size  | Value  | (Type,Locality) | Section || FileCtx |");
	rtrace("---------------------------------------------------------------------------------------");
	for (uint32_t i = 0; i < symbTable->count; i++) {
		AOEFFSymEnt* symbEnt = &symbTable->symbols[i];
	
		switch (SE_GET_LOC(symbEnt->seSymbInfo)) {
			case SE_LOCAL:
				locStr = "LOCAL";
				break;
			case SE_GLOBL:
				locStr = "GLOBL";
				break;
			default:
				locStr = "UNKWN";
				break;
		}

		switch (SE_GET_TYPE(symbEnt->seSymbInfo)) {
			case SE_NONE_T:
				typeStr = "NONE";
				break;
			case SE_ABSV_T:
				typeStr = "ABSV";
				break;
			case SE_FUNC_T:
				typeStr = "FUNC";
				break;
			case SE_OBJ_T:
				typeStr = "OBJ";
				break;
			default:
				typeStr = "UNKWN";
				break;
		}

		char* symbName = &symbTable->SymbolStringTable.strTab.stStrs[symbEnt->seSymbName];
		rtrace("%2d | %10d | %10s | 0x%04X | 0x%04X |  (%s, %s)  |   %2d    ||   %2d    |", i, symbEnt->seSymbName, symbName, 
				symbEnt->seSymbSize, symbEnt->seSymbVal, typeStr, locStr, symbEnt->seSymbSect, symbTable->filectxIndices[i]);
		rtrace("---------------------------------------------------------------------------------------");
	}
}

uint32_t appendString(SymbolTable* symbTable, const char* str) {
	char* strs = symbTable->SymbolStringTable.strTab.stStrs;

	size_t len = strlen(str) + 1; // +1 for null terminator
	if (symbTable->SymbolStringTable.strbCount + len == symbTable->SymbolStringTable.strbCap) {
		symbTable->SymbolStringTable.strbCap *= 2;

		strs = (char*) realloc(strs,  sizeof(char) * symbTable->SymbolStringTable.strbCap);
		if (!strs) emitError(ERR_MEM, "Failed to reallocate memory for symbol string table");
		symbTable->SymbolStringTable.strTab.stStrs = strs;
	}

	char* dest = &strs[symbTable->SymbolStringTable.strbCount];

	// When the string table is empty, the first string goes at the start
	// Otherwise, it goes after the null terminator of the previous string
	if (symbTable->SymbolStringTable.strCount != 0) {
		// dest should be the null terminator of the previous string
		if (*dest != '\0') emitError(ERR_INTERNAL, "Symbol string table is corrupted (missing null terminator)");
		// dest++; // Begin copying after null
	}
	uint32_t index = symbTable->SymbolStringTable.strbCount;
	strcpy(dest, str);
	symbTable->SymbolStringTable.strbCount += len;
	symbTable->SymbolStringTable.strCount += 1;

	rlog("Appended string to symbol string table: %s (index %d)", str, index);

	return index;
}