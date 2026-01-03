#ifndef _RELOC_TABLE_H
#define _RELOC_TABLE_H

#include "aoef.h"


/**
 * A wrapper around the AOEFF relocation table structure.
 */
typedef struct RelocTable {
	struct {
		AOEFFTRelTab* tables;
		int count;
		int cap;

		int* filectxIndices; // Indices of the file contexts each static relocation table belongs to
		// Note that this ties in the table to its file
		// The relocated symbols may not necessarily be for the same file
		// Main example are externs
		// If index is not -1, it refers to a symbol in the symbol table
		// That symbol has its file context index, which refers to the file it is defined at
	} trelocs;

	struct {
		AOEFFDRelTab* tables;
		int count;
		int cap;

		int* filectxIndices; // Indices of the file contexts each dynamic relocation table belongs to
	} drelocs;

	struct {
		AOEFFRelStrTab strTab;
		uint32_t strbCount; // The number of bytes used in the relocation string table
		uint32_t strbCap;   // The capacity of the relocation string table in bytes

		uint32_t strCount; // The number of strings in the table
	} RelocStringTable;
} RelocTable;


RelocTable* initRelocTable();
void deinitRelocTable(RelocTable* relocTable);

void appendTRelocTable(RelocTable* relocTable, AOEFFTRelTab* treloc, int filectxIndex);
void appendDRelocTable(RelocTable* relocTable, AOEFFDRelTab* dreloc);

void displayRelocTable(RelocTable* relocTable);

/* For the embedded relocation string table */

/**
 * Appends a string to the relocation table's string table. Note that this maintains the null-terminator of the previous string.
 * Returns the index of the newly appended string in the string table for the relocation table's relTabName field.
 * @param relocTable The relocation table containing the string table
 * @param str The string to append
 * @return The index of the newly appended string in the string table
 */
uint32_t appendRelocString(RelocTable* relocTable, const char* str);

#endif