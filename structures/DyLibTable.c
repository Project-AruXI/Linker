#include <stdlib.h>
#include <string.h>

#include "DyLibTable.h"
#include "diagnostics.h"


DyLibTable* initDyLibTable() {
	DyLibTable* dylibTable = (DyLibTable*) malloc(sizeof(DyLibTable));
	if (!dylibTable) emitError(ERR_MEM, "Failed to allocate memory for dynamic library table");

	dylibTable->entries = (DyLibTableEntry*) calloc(10, sizeof(DyLibTableEntry));
	if (!dylibTable->entries) emitError(ERR_MEM, "Failed to allocate memory for dynamic library table entries");

	dylibTable->count = 0;
	dylibTable->cap = 10;

	return dylibTable;
}

void deinitDyLibTable(DyLibTable* table) {
	for (uint32_t i = 0; i < table->count; i++) {
		free(table->entries[i].lib);
		free(table->entries[i].symbols);
	}
	free(table->entries);
	free(table);
}

static DyLibTableEntry* addDyLib(DyLibTable* dylibTable, const char* libName) {
	if (dylibTable->count >= dylibTable->cap) {
		dylibTable->cap *= 2;
		dylibTable->entries = (DyLibTableEntry*) realloc(dylibTable->entries, dylibTable->cap * sizeof(DyLibTableEntry));
		if (!dylibTable->entries) emitError(ERR_MEM, "Failed to reallocate memory for dynamic library table entries");
	}

	DyLibTableEntry* newEntry = &dylibTable->entries[dylibTable->count++];
	newEntry->lib = strdup(libName);
	if (!newEntry->lib) emitError(ERR_MEM, "Failed to allocate memory for dynamic library name");

	newEntry->symbols = (AOEFFSymbEntry*) malloc(10 * sizeof(AOEFFSymbEntry));
	if (!newEntry->symbols) emitError(ERR_MEM, "Failed to allocate memory for dynamic library symbols");

	newEntry->symbCount = 0;
	newEntry->symbCap = 10;

	return newEntry;
}

DyLibTableEntry* addDyLibSymb(DyLibTable* dylibTable, const char* libName, AOEFFSymbEntry* symbol, const char* symbName) {
	DyLibTableEntry* dylib = NULL;

	// Check if the library already exists in the table
	for (uint32_t i = 0; i < dylibTable->count; i++) {
		if (strcmp(dylibTable->entries[i].lib, libName) == 0) {
			dylib = &dylibTable->entries[i];
			break;
		}
	}

	if (!dylib) dylib = addDyLib(dylibTable, libName);

	// Add the symbol to the library entry
	if (dylib->symbCount >= dylib->symbCap) {
		dylib->symbCap *= 2;
		dylib->symbols = (AOEFFSymbEntry*) realloc(dylib->symbols, dylib->symbCap * sizeof(AOEFFSymbEntry));
		if (!dylib->symbols) emitError(ERR_MEM, "Failed to reallocate memory for dynamic library symbols");

		// Since the symbol names are parallel to the symbols, resize it as well
		dylib->symbNames = (char**) realloc(dylib->symbNames, dylib->symbCap * sizeof(char*));
		if (!dylib->symbNames) emitError(ERR_MEM, "Failed to reallocate memory for dynamic library symbol names");
	}
	dylib->symbols[dylib->symbCount++] = *symbol;

	// Add the name
	dylib->symbNames[dylib->symbCount - 1] = strdup(symbName);
	if (!dylib->symbNames[dylib->symbCount - 1]) emitError(ERR_MEM, "Failed to allocate memory for dynamic library symbol name");

	return dylib;
}

void showDyLibTable(DyLibTable* table) {
	for (uint32_t i = 0; i < table->count; i++) {
		DyLibTableEntry* entry = &table->entries[i];
		rlog("Library: %s\n", entry->lib);
		rlog("  Symbols:\n");
		for (uint32_t j = 0; j < entry->symbCount; j++) {
			rlog("    Symbol %u: Name Index: %u, Name: %s, Size: %u, Value: %u, Info: %u, Section: %u\n",
				j,
				entry->symbols[j].seSymbName,
				entry->symbNames[j],
				entry->symbols[j].seSymbSize,
				entry->symbols[j].seSymbVal,
				entry->symbols[j].seSymbInfo,
				entry->symbols[j].seSymbSect
			);
		}
	}
}