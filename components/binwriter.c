#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "binwriter.h"
#include "aoef.h"
#include "diagnostics.h"



static uint32_t getTRelTabSize(AOEFFTRelTab* relocTables, uint32_t relTabCount) {
	// Number of bytes that the whole relocation stuff uses
	// This means that there is the size of relSect + relTabName first
	// The after that, there is the array of entries, which is takes up number of entries (relCount) * size of each entry 
	// relCount then follows that array
	// All of this is per table with relTabCount tables

	uint32_t totalSize = 0;

	for (uint32_t i = 0; i < relTabCount; i++) {
		AOEFFTRelTab* tab = &relocTables[i];
		totalSize += sizeof(uint8_t); // relSect
		totalSize += 3; // padding
		totalSize += sizeof(uint32_t); // relTabName
		totalSize += sizeof(uint32_t); // relCount
		totalSize += 4; // padding
		totalSize += sizeof(AOEFFTRelEnt) * tab->relCount; // relEntries
	}

	return totalSize;
}

static uint32_t getEntrySymbolAddress(SymbolTable* symbTable, Config* config) {
	if (config->isDynamic || config->isKernel) return 0x00000000; // Dynamic libraries and kernels do not have entry points

	char* entrySymbolName = NULL;
	if (config->useStdLib) entrySymbolName = "main";
	else entrySymbolName = "_init";

	int entrySymbIndex = getSymbolByName(symbTable, entrySymbolName, 0);
	if (entrySymbIndex == -1) emitError(ERR_UNDEFINED, "Entry point symbol %s is undefined", entrySymbolName);

	AOEFFSymEnt* entrySymb = &symbTable->symbols[entrySymbIndex];
	return entrySymb->seSymbVal;
}

AOEFFSectHdr* normalizeSectionHeaders(SectionTable* sectTable, uint32_t sectOff) {
	// Updates the offset fields of the section headers to reflect their actual offsets in the final binary
	// Also adds a blank section header at the end

	int sectCount = 0;
	for (int i = 0; i < 4; i++) {
		if (sectTable->sections[i].shSectSize != 0) sectCount++;
	}
	sectCount++; // For the blank ending entry

	AOEFFSectHdr* newSectHeaders = (AOEFFSectHdr*) calloc(sectCount, sizeof(AOEFFSectHdr));
	if (!newSectHeaders) emitError(ERR_MEM, NULL, "Failed to allocate memory for normalized section headers.");

	// Offset where all sections start at, basically the end of the dynamic relocation tables
	uint32_t baseOffset = sectOff;
	rlog("Base section for all section data: 0x%x", baseOffset);
	uint32_t sectOffset = baseOffset;
	for (int i = 0, hdrIdx = 0; i < 4; i++) {
		if (sectTable->sections[i].shSectSize == 0) continue;

		newSectHeaders[hdrIdx] = sectTable->sections[i];
		newSectHeaders[hdrIdx].shSectOff = sectOffset;

		if (i == 2) newSectHeaders[hdrIdx].shSectOff = 0; // .bss has no offset

		log("Section %s: Offset 0x%x, Size 0x%x", newSectHeaders[hdrIdx].shSectName, newSectHeaders[hdrIdx].shSectOff, newSectHeaders[hdrIdx].shSectSize);

		if (i != 2) sectOffset += newSectHeaders[hdrIdx].shSectSize;
		hdrIdx++;
	}

	return newSectHeaders;
}

static AOEFFSymEnt* normalizeSymbolTable(SymbolTable* symbTable) {
	// Insert blank entry

	uint32_t newSize = symbTable->count + 1;
	AOEFFSymEnt* newSymbEntries = (AOEFFSymEnt*) calloc(newSize, sizeof(AOEFFSymEnt));
	if (!newSymbEntries) emitError(ERR_MEM, NULL, "Failed to allocate memory for normalized symbol table.");

	// Transfer old entries
	memcpy(newSymbEntries, symbTable->symbols, sizeof(AOEFFSymEnt) * symbTable->count);

	// The last entry is already zeroed out due to calloc

	// Insert ending string "END_AOEFF_STRS\0" after last string (keeping its null-terminator)
	appendString(symbTable, "END_AOEFF_STRS\0");
	// Note that there are two nulls, appendString only added one, manually add the other one
	char* strs = symbTable->SymbolStringTable.strTab.stStrs;
	memcpy(&strs[symbTable->SymbolStringTable.strbCount], "\0", 1);
	symbTable->SymbolStringTable.strbCount += 1;

	return newSymbEntries;
}

void writeBinary(Config* config, GlobalTables* globalTables) {
	initScope("writeBinary");

	FILE* outfile = fopen(config->outfile, "wb");
	if (!outfile) emitError(ERR_IO, NULL, "Failed to open output file %s for writing.", config->outfile);

	int sectEntries = 0;
	for (int i = 0; i < 4; i++) {
		if (globalTables->sectionTable->sections[i].shSectSize != 0) sectEntries++;
	}
	sectEntries++; // Ending blank entry

	uint32_t symbTableSize = globalTables->symbolTable->count;
	symbTableSize++; // Ending blank entry

	uint32_t symbOff = sizeof(AOEFFhdr) + (sizeof(AOEFFSectHdr) * sectEntries);

	uint32_t strTabOff = symbOff + (sizeof(AOEFFSymEnt) * symbTableSize);
	uint32_t strTabSize = globalTables->symbolTable->SymbolStringTable.strbCount;
	strTabSize += 16; // Ending string is 16 bytes

	uint32_t relStrOff = strTabOff + strTabSize;
	// uint32_t relStrSize = globalTables->relocTable->RelocStringTable.strbCount;
	uint32_t relStrSize = 0;

	// uint32_t trelTabCount = globalTables->relocTable->trelocs.count;
	// uint32_t trelTabOff = relStrOff + relStrSize;
	// uint32_t trelTabSize = getTRelTabSize(globalTables->relocTable->trelocs.tables, trelTabCount);
	uint32_t trelTabCount = 0;
	uint32_t trelTabOff = relStrOff + relStrSize;
	uint32_t trelTabSize = 0;

	uint32_t drelTabCount = globalTables->relocTable->drelocs.count;
	uint32_t drelTabOff = trelTabOff + trelTabSize;
	uint32_t drelTabSize = 0; // TODO: implement dynamic relocation table

	// TODO: Dynamic library table info

	rlog("trelTabOff: 0x%x; trelTabSize: 0x%x", trelTabOff, trelTabSize);
	// rlog("drelTabOff: 0x%x; drelTabSize: 0x%x", drelTabOff, drelTabSize);

	// Write header info
	AOEFFhdr header = {
		.hID = {AH_ID0, AH_ID1, AH_ID2, AH_ID3},
		.hType = AHT_EXEC, // For now, make it be exe, implement other types later
		.hEntry = getEntrySymbolAddress(globalTables->symbolTable, config),
		.hSectOff = sizeof(AOEFFhdr),
		.hSectSize = sectEntries,
		.hSymbOff = symbOff,
		.hSymbSize = symbTableSize,
		.hStrTabOff = strTabOff,
		.hStrTabSize = strTabSize,
		.hRelStrTabOff = relStrOff,
		.hRelStrTabSize = relStrSize,
		.hTRelTabOff = trelTabOff,
		.hTRelTabSize = trelTabCount,
		.hDRelTabOff = drelTabOff,
		.hDRelTabSize = drelTabCount,
		.hDyLibTabOff = 0, // TODO: implement dynamic stuff
		.hDyLibTabSize = 0,
		.hDyLibStrTabOff = 0,
		.hDyLibStrTabSize = 0,
		.hImportTabOff = 0,
		.hImportTabSize = 0
	};
	fwrite(&header, sizeof(AOEFFhdr), 1, outfile);

	// Write section headers
	AOEFFSectHdr* sectHeader = normalizeSectionHeaders(globalTables->sectionTable, drelTabOff + drelTabSize);
	fwrite(sectHeader, sizeof(AOEFFSectHdr), sectEntries, outfile);
	free(sectHeader);

	// Write symbol table
	AOEFFSymEnt* symbEntries = normalizeSymbolTable(globalTables->symbolTable);
	fwrite(symbEntries, sizeof(AOEFFSymEnt), symbTableSize, outfile);
	free(symbEntries);

	// Write string table
	fwrite(globalTables->symbolTable->SymbolStringTable.strTab.stStrs, sizeof(char), strTabSize, outfile);

	// Write relocation string table
	fwrite(globalTables->relocTable->RelocStringTable.strTab.rstStrs, sizeof(char), relStrSize, outfile);

	uint8_t zeroBufferPadding[4] = {0};

	// Write static relocation tables
	// for (uint32_t i = 0; i < trelTabCount; i++) {
	// 	AOEFFTRelTab* tab = &globalTables->relocTable->trelocs.tables[i];
	// 	rlog("Writing TReloc Table %d: relSect=0x%x, relTabName=0x%x, relCount=%d", i, tab->relSect, tab->relTabName, tab->relCount);
	// 	fwrite(&tab->relSect, sizeof(uint8_t), 1, outfile);
	// 	fwrite(zeroBufferPadding, 3, 1, outfile); // padding
	// 	fwrite(&tab->relTabName, sizeof(uint32_t), 1, outfile);
	// 	fwrite(&tab->relCount, sizeof(uint32_t), 1, outfile);
	// 	fwrite(zeroBufferPadding, 4, 1, outfile); // padding
	// 	fwrite(tab->relEntries, sizeof(AOEFFTRelEnt), tab->relCount, outfile);
	// }

	// Write dynamic relocation tables
	for (uint32_t i = 0; i < drelTabCount; i++) {
		AOEFFDRelTab* tab = &globalTables->relocTable->drelocs.tables[i];
		rlog("Writing DReloc Table %d: relSect=0x%x, relTabName=0x%x, relCount=%d", i, tab->relSect, tab->relTabName, tab->relCount);
		fwrite(&tab->relSect, sizeof(uint8_t), 1, outfile);
		fwrite(zeroBufferPadding, 3, 1, outfile); // padding
		fwrite(&tab->relTabName, sizeof(uint32_t), 1, outfile);
		fwrite(&tab->relCount, sizeof(uint32_t), 1, outfile);
		fwrite(zeroBufferPadding, 4, 1, outfile); // padding
		fwrite(tab->relEntries, sizeof(AOEFFDRelEnt), tab->relCount, outfile);
	}

	// TODO: write dynamic library table, write import table

	// Write payload
	for (int i = 0; i < 4; i++) {
		AOEFFSectHdr* sectHdr = &globalTables->sectionTable->sections[i];
		if (sectHdr->shSectSize == 0) continue;

		rlog("Writing section %s at offset 0x%x, size 0x%x", sectHdr->shSectName, sectHdr->shSectOff, sectHdr->shSectSize);
		if (strcmp(sectHdr->shSectName, ".data") == 0) {
			log("Writing .data section");
			fwrite(globalTables->sectionTable->_data, sizeof(uint8_t), sectHdr->shSectSize, outfile);
		} else if (strcmp(sectHdr->shSectName, ".const") == 0) {
			log("Writing .const section");
			fwrite(globalTables->sectionTable->_const, sizeof(uint8_t), sectHdr->shSectSize, outfile);
		} else if (strcmp(sectHdr->shSectName, ".text") == 0) {
			log("Writing .text section");
			fwrite(globalTables->sectionTable->_text, sizeof(uint32_t), sectHdr->shSectSize / 4, outfile);
		} else {
			emitError(ERR_INTERNAL, NULL, "Writing section %s is not implemented yet.", sectHdr->shSectName);
		}
	}

	fclose(outfile);
}