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
	} trelocs;

	struct {
		AOEFFDRelTab* tables;
		int count;
		int cap;
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

void appendTRelocTable(RelocTable* relocTable, AOEFFTRelTab* treloc);
void appendDRelocTable(RelocTable* relocTable, AOEFFDRelTab* dreloc);

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