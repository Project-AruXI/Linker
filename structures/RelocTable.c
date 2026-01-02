#include <stdlib.h>
#include <string.h>

#include "RelocTable.h"
#include "diagnostics.h"


RelocTable* initRelocTable() {
	RelocTable* relocTable = (RelocTable*) malloc(sizeof(RelocTable));
	if (!relocTable) emitError(ERR_MEM, "Failed to allocate memory for relocation table");

	relocTable->trelocs.tables = (AOEFFTRelTab*) malloc(sizeof(AOEFFTRelTab) * 10);
	if (!relocTable->trelocs.tables) emitError(ERR_MEM, "Failed to allocate memory for static relocation tables");

	relocTable->trelocs.count = 0;
	relocTable->trelocs.cap = 10;

	relocTable->drelocs.tables = (AOEFFDRelTab*) malloc(sizeof(AOEFFDRelTab) * 10);
	if (!relocTable->drelocs.tables) emitError(ERR_MEM, "Failed to allocate memory for dynamic relocation tables");

	relocTable->drelocs.count = 0;
	relocTable->drelocs.cap = 10;


	char* relStrTabData = (char*) malloc(sizeof(char) * 50);
	if (!relStrTabData) emitError(ERR_MEM, "Failed to allocate memory for relocation string table");

	relocTable->RelocStringTable.strTab.rstStrs = relStrTabData;
	relocTable->RelocStringTable.strbCount = 0;
	relocTable->RelocStringTable.strbCap= 50;
	relocTable->RelocStringTable.strCount = 0;

	return relocTable;
}

void deinitRelocTable(RelocTable* relocTable) {
}

void appendTRelocTable(RelocTable* relocTable, AOEFFTRelTab* treloc) {
	if (relocTable->trelocs.count == relocTable->trelocs.cap) {
		relocTable->trelocs.cap *= 2;
		AOEFFTRelTab* newTables = (AOEFFTRelTab*) realloc(relocTable->trelocs.tables, sizeof(AOEFFTRelTab) * relocTable->trelocs.cap);
		if (!newTables) emitError(ERR_MEM, "Failed to reallocate memory for static relocation tables");
		relocTable->trelocs.tables = newTables;
	}

	relocTable->trelocs.tables[relocTable->trelocs.count].relCount = treloc->relCount;
	relocTable->trelocs.tables[relocTable->trelocs.count].relSect = treloc->relSect;
	relocTable->trelocs.tables[relocTable->trelocs.count].relTabName = treloc->relTabName;

	// Make deep copies of the entries
	relocTable->trelocs.tables[relocTable->trelocs.count].relEntries = (AOEFFTRelEnt*) malloc(sizeof(AOEFFTRelEnt) * treloc->relCount);
	if (!relocTable->trelocs.tables[relocTable->trelocs.count].relEntries) emitError(ERR_MEM, "Failed to allocate memory for static relocation entries");
	for (uint32_t i = 0; i < treloc->relCount; i++) {
		relocTable->trelocs.tables[relocTable->trelocs.count].relEntries[i] = treloc->relEntries[i];
		// NOTE TO SELF: The reSymb field is the index into the symbol table, which very easily might become out of sync
		// This will be updated after
	}

	relocTable->trelocs.count += 1;
}
void appendDRelocTable(RelocTable* relocTable, AOEFFDRelTab* dreloc) {

}


void appendRelocString(RelocTable* relocTable, const char* str) {
	char* strs = relocTable->RelocStringTable.strTab.rstStrs;

	size_t len = strlen(str) + 1; // +1 for null terminator
	if (relocTable->RelocStringTable.strbCount + len == relocTable->RelocStringTable.strbCap) {
		relocTable->RelocStringTable.strbCap *= 2;

		strs = (char*) realloc(strs,  sizeof(char) * relocTable->RelocStringTable.strbCap);
		if (!strs) emitError(ERR_MEM, "Failed to reallocate memory for relocation string table");
		relocTable->RelocStringTable.strTab.rstStrs = strs;
	}

	uint32_t index = relocTable->RelocStringTable.strbCount;

	char* dest = &strs[relocTable->RelocStringTable.strbCount];
	// dest should be the null terminator of the previous string
	if (*dest != '\0') emitError(ERR_INTERNAL, "Relocation string table is corrupted (missing null terminator)");
	dest++; // Begin copying after null
	strcpy(dest, str);
	relocTable->RelocStringTable.strbCount += len;
	relocTable->RelocStringTable.strCount += 1;

	return index;
}