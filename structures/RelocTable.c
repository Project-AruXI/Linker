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

	relocTable->trelocs.filectxIndices = (int*) malloc(sizeof(int) * 10);
	if (!relocTable->trelocs.filectxIndices) emitError(ERR_MEM, "Failed to allocate memory for static relocation table file context indices");


	int** unresolvedIndices = (int**) malloc(sizeof(int*) * 10);
	if (!unresolvedIndices) emitError(ERR_MEM, "Failed to allocate memory for unresolved relocation indices");

	relocTable->unresolved.unresolvedIndices = unresolvedIndices;
	relocTable->unresolved.count = 0;
	relocTable->unresolved.cap = 10;


	relocTable->drelocs.count = 0;


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

AOEFFDRelTab* initDRelocTable(uint8_t sect, uint32_t nameIndex) {
	// For dynamic relocation tables, the section will be either text (for fjt) or data (for djt)

	AOEFFDRelTab* drelocTable = (AOEFFDRelTab*) malloc(sizeof(AOEFFDRelTab));
	if (!drelocTable) emitError(ERR_MEM, "Failed to allocate memory for dynamic relocation table");

	drelocTable->relSect = sect;
	drelocTable->relTabName = nameIndex;
	drelocTable->relCount = 0;
	drelocTable->relEntries = NULL;

	return drelocTable;
}
void deinitDRelocTable(AOEFFDRelTab* drelocTable) {}

AOEFFDRelEnt* initDRelocEntry(uint32_t off, uint32_t symb, uint8_t type, int32_t addend) {
	AOEFFDRelEnt* drelocEntry = (AOEFFDRelEnt*) malloc(sizeof(AOEFFDRelEnt));
	if (!drelocEntry) emitError(ERR_MEM, "Failed to allocate memory for dynamic relocation entry");

	drelocEntry->reOff = off;
	drelocEntry->reSymb = symb;
	drelocEntry->reType = RE_ARU32_DECOMP; // Assuming all dynamic relocations are of this type
	drelocEntry->reAddend = addend;

	return drelocEntry;
}
void deinitDRelocEntry(AOEFFDRelEnt* drelocEntry) {}

void appendDRelocEntry(AOEFFDRelTab* drelocTable, AOEFFDRelEnt* drelocEntry) {
	drelocTable->relEntries = (AOEFFDRelEnt*) realloc(drelocTable->relEntries, sizeof(AOEFFDRelEnt) * (drelocTable->relCount + 1));
	if (!drelocTable->relEntries) emitError(ERR_MEM, "Failed to reallocate memory for dynamic relocation entries");

	drelocTable->relEntries[drelocTable->relCount] = *drelocEntry;
	drelocTable->relCount += 1;
}

void appendTRelocTable(RelocTable* relocTable, AOEFFTRelTab* treloc, int filectxIndex) {
	if (relocTable->trelocs.count == relocTable->trelocs.cap) {
		relocTable->trelocs.cap *= 2;
		AOEFFTRelTab* newTables = (AOEFFTRelTab*) realloc(relocTable->trelocs.tables, sizeof(AOEFFTRelTab) * relocTable->trelocs.cap);
		if (!newTables) emitError(ERR_MEM, "Failed to reallocate memory for static relocation tables");

		int* newFilectxIndices = (int*) realloc(relocTable->trelocs.filectxIndices, sizeof(int) * relocTable->trelocs.cap);
		if (!newFilectxIndices) emitError(ERR_MEM, "Failed to reallocate memory for static relocation table file context indices");

		relocTable->trelocs.tables = newTables;
		relocTable->trelocs.filectxIndices = newFilectxIndices;
	}

	relocTable->trelocs.tables[relocTable->trelocs.count].relCount = treloc->relCount;
	relocTable->trelocs.tables[relocTable->trelocs.count].relSect = treloc->relSect;
	relocTable->trelocs.tables[relocTable->trelocs.count].relTabName = treloc->relTabName;
	relocTable->trelocs.filectxIndices[relocTable->trelocs.count] = filectxIndex;

	// Make deep copies of the entries
	relocTable->trelocs.tables[relocTable->trelocs.count].relEntries = (AOEFFTRelEnt*) malloc(sizeof(AOEFFTRelEnt) * treloc->relCount);
	if (!relocTable->trelocs.tables[relocTable->trelocs.count].relEntries) emitError(ERR_MEM, "Failed to allocate memory for static relocation entries");

	// TODO: Add this to the documentation since it is very important
	// Due to how AOEFFTRelTab is defined, it uses a pointer for the entries
	// This is used to store the data
	// However, on disk, the pointer is not a true pointer but rather signifies an array of entries
	// But when doing `treloc->relEntries`, it is treated as a pointer (aka it gets the pointer itself, not the data)
	// So we need to cast it to the correct type to get the data
	// The start of the data can be seen as where the pointer is located at
	AOEFFTRelEnt* trueEntries = (AOEFFTRelEnt*) &treloc->relEntries;

	for (uint32_t i = 0; i < treloc->relCount; i++) {
		relocTable->trelocs.tables[relocTable->trelocs.count].relEntries[i] = trueEntries[i];
		// NOTE TO SELF: The reSymb field is the index into the symbol table, which very easily might become out of sync
		// This will be updated after
		// Another note: reOff is very important to be kept locally and not changed
		// This is because it is the offset from the start of the section, which is needed for relocation
		// Especially for branching where the offset is calculated from the instruction's location
		trace("Copied relocation entry %d: off=0x%X, symb=%d, type=%d, addend=0x%X", i,
		      trueEntries[i].reOff,
		      trueEntries[i].reSymb,
		      trueEntries[i].reType,
		      trueEntries[i].reAddend);
	}

	relocTable->trelocs.count += 1;
}

void appendDRelocTable(RelocTable* relocTable, AOEFFDRelTab* dreloc) {
	// Append the relocation table to the next available slot
	// There are only two slots, one for fjt and one for djt
	// Check if the table already exists (ie if the table is for fjt, and there is already a fjt table)
	//   If so, only copy over the entries
	//   If not, add it to the next available slot
	// Identify the table type based on the section

	int tableIndex = -1;
	if (dreloc->relSect == 3) { // .text section, fjt
		tableIndex = 0;
	} else if (dreloc->relSect == 0) { // .data section, djt
		tableIndex = 1;
	} else {
		emitError(ERR_INVALID_FORMAT, "Dynamic relocation table has invalid section %d", dreloc->relSect);
	}

	if (relocTable->drelocs.count > tableIndex) {
		// Table already exists, only copy over entries
		AOEFFDRelTab* existingTable = &relocTable->drelocs.tables[tableIndex];

		existingTable->relEntries = (AOEFFDRelEnt*) realloc(existingTable->relEntries, sizeof(AOEFFDRelEnt) * (existingTable->relCount + dreloc->relCount));
		if (!existingTable->relEntries) emitError(ERR_MEM, "Failed to reallocate memory for dynamic relocation entries");

		for (uint32_t i = 0; i < dreloc->relCount; i++) {
			existingTable->relEntries[existingTable->relCount + i] = dreloc->relEntries[i];
			trace("Appended dynamic relocation entry %d to existing table: off=0x%X, symb=%d, type=%d, addend=0x%X", i,
					dreloc->relEntries[i].reOff,
					dreloc->relEntries[i].reSymb,
					dreloc->relEntries[i].reType,
					dreloc->relEntries[i].reAddend);
		}

		existingTable->relCount += dreloc->relCount;
	} else {
		// New table, add it
		relocTable->drelocs.tables[tableIndex].relSect = dreloc->relSect;
		relocTable->drelocs.tables[tableIndex].relTabName = dreloc->relTabName;
		relocTable->drelocs.tables[tableIndex].relCount = dreloc->relCount;

		// Deep copy entries
		relocTable->drelocs.tables[tableIndex].relEntries = (AOEFFDRelEnt*) malloc(sizeof(AOEFFDRelEnt) * dreloc->relCount);
		if (!relocTable->drelocs.tables[tableIndex].relEntries) emitError(ERR_MEM, "Failed to allocate memory for dynamic relocation entries");

		for (uint32_t i = 0; i < dreloc->relCount; i++) {
			relocTable->drelocs.tables[tableIndex].relEntries[i] = dreloc->relEntries[i];
			trace("Copied dynamic relocation entry %d to new table: off=0x%X, symb=%d, type=%d, addend=0x%X", i,
					dreloc->relEntries[i].reOff,
					dreloc->relEntries[i].reSymb,
					dreloc->relEntries[i].reType,
					dreloc->relEntries[i].reAddend);
		}
	}

	relocTable->drelocs.count = (tableIndex + 1 > relocTable->drelocs.count) ? tableIndex + 1 : relocTable->drelocs.count;
}

void addUnresolved(RelocTable* relocTable, int relocTableIndex, int relocEntryIndex) {
	if (relocTable->unresolved.count == relocTable->unresolved.cap) {
		relocTable->unresolved.cap *= 2;
		int** newUnresolvedIndices = (int**) realloc(relocTable->unresolved.unresolvedIndices, sizeof(int*) * relocTable->unresolved.cap);
		if (!newUnresolvedIndices) emitError(ERR_MEM, "Failed to reallocate memory for unresolved symbol indices");

		relocTable->unresolved.unresolvedIndices = newUnresolvedIndices;
	}

	int* newIndexPair = (int*) malloc(sizeof(int) * 2);
	if (!newIndexPair) emitError(ERR_MEM, "Failed to allocate memory for unresolved symbol index pair");

	newIndexPair[0] = relocTableIndex;
	newIndexPair[1] = relocEntryIndex;

	relocTable->unresolved.unresolvedIndices[relocTable->unresolved.count] = newIndexPair;
	relocTable->unresolved.count += 1;
}


void displayRelocTable(RelocTable* relocTable) {
	char* typeStr = NULL;

	rtrace("====== Relocation Tables ======");

	rtrace("-- Static Relocation Tables --");
	rtrace("Total Tables: %d", relocTable->trelocs.count);
	for (int i = 0; i < relocTable->trelocs.count; i++) {
		AOEFFTRelTab* table = &relocTable->trelocs.tables[i];

		char* relTabName = &relocTable->RelocStringTable.strTab.rstStrs[table->relTabName];

		rtrace("---------------- Table %d -----------------", i);
		rtrace("Section: %d | Name Index: %d | Name: %s | Entry Count: %d || FileCtx: %d |", table->relSect, table->relTabName, relTabName, table->relCount, relocTable->trelocs.filectxIndices[i]);
		rtrace("------------------------------------------");
		rtrace(" Num |  Offset  | Symbol | Type | Addend |");
		rtrace("------------------------------------------");
		for (uint32_t j = 0; j < table->relCount; j++) {
			AOEFFTRelEnt* entry = &table->relEntries[j];

			switch (entry->reType) {
				case RE_ARU32_ABS14:
					typeStr = "ABS14";
					break;
				case RE_ARU32_MEM9:
					typeStr = "MEM9";
					break;
				case RE_ARU32_IR24:
					typeStr = "IR24";
					break;
				case RE_ARU32_IR19:
					typeStr = "IR19";
					break;
				case RE_ARU32_DECOMP:
					typeStr = "DECOMP";
					break;
				default:
					typeStr = "UNKNOWN";
					break;
			}

			rtrace("%4d | 0x%06X |  %5d | %4s | 0x%04X |", j, entry->reOff, entry->reSymb, typeStr, entry->reAddend);
		}
		rtrace("------------------------------------------\n");
	}

	rtrace("-- Dynamic Relocation Tables --");
	rtrace("Total Tables: %d%c", relocTable->drelocs.count, (relocTable->drelocs.count != 0) ? '\0' : '\n');
	for (int i = 0; i < relocTable->drelocs.count; i++) {
		AOEFFDRelTab* table = &relocTable->drelocs.tables[i];

		char* relTabName = &relocTable->RelocStringTable.strTab.rstStrs[table->relTabName];

		rtrace("---------------- Table %d -----------------", i);
		rtrace("Section: %d | Name Index: %d | Name: %s | Entry Count: %d", table->relSect, table->relTabName, relTabName, table->relCount);
		rtrace("------------------------------------------");
		rtrace(" Num |  Offset  | Symbol | Type | Addend |");
		rtrace("------------------------------------------");
		for (uint32_t j = 0; j < table->relCount; j++) {
			AOEFFDRelEnt* entry = &table->relEntries[j];

			switch (entry->reType) {
				case RE_ARU32_ABS14:
					typeStr = "ABS14";
					break;
				case RE_ARU32_MEM9:
					typeStr = "MEM9";
					break;
				case RE_ARU32_IR24:
					typeStr = "IR24";
					break;
				case RE_ARU32_IR19:
					typeStr = "IR19";
					break;
				case RE_ARU32_DECOMP:
					typeStr = "DECOMP";
					break;
				default:
					typeStr = "UNKNOWN";
					break;
			}

			rtrace("%4d | 0x%06X |  %5d | %4s | 0x%04X |", j, entry->reOff, entry->reSymb, typeStr, entry->reAddend);
		}
		rtrace("------------------------------------------\n");
	}
}

uint32_t appendRelocString(RelocTable* relocTable, const char* str) {
	char* strs = relocTable->RelocStringTable.strTab.rstStrs;

	size_t len = strlen(str) + 1; // +1 for null terminator
	if (relocTable->RelocStringTable.strbCount + len >= relocTable->RelocStringTable.strbCap) {
		relocTable->RelocStringTable.strbCap *= 2;

		strs = (char*) realloc(strs,  sizeof(char) * relocTable->RelocStringTable.strbCap);
		if (!strs) emitError(ERR_MEM, "Failed to reallocate memory for relocation string table");
		relocTable->RelocStringTable.strTab.rstStrs = strs;
	}

	char* dest = &strs[relocTable->RelocStringTable.strbCount];
	
	// When the string table is empty, the first string goes at the start
	// Otherwise, it goes after the null terminator of the previous string
	if (relocTable->RelocStringTable.strCount != 0) {
		// dest should be the null terminator of the previous string
		if (*dest != '\0') emitError(ERR_INTERNAL, "Relocation string table is corrupted (missing null terminator)");
		// dest++; // Begin copying after null
	}
	uint32_t index = relocTable->RelocStringTable.strbCount;
	strcpy(dest, str);
	relocTable->RelocStringTable.strbCount += len;
	relocTable->RelocStringTable.strCount += 1;

	rlog("Appended string to relocation string table: %s (index %d)", str, index);

	return index;
}