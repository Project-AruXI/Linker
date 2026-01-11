#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>

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

static uint32_t getDRelTabSize(AOEFFDRelTab* relocTables, uint32_t relTabCount) {
	// Similar to static relocation tables, but with dynamic relocation entries

	uint32_t totalSize = 0;

	for (uint32_t i = 0; i < relTabCount; i++) {
		AOEFFDRelTab* tab = &relocTables[i];
		totalSize += sizeof(uint8_t); // relSect
		totalSize += 3; // padding
		totalSize += sizeof(uint32_t); // relTabName
		totalSize += sizeof(uint32_t); // relCount
		totalSize += 4; // padding
		totalSize += sizeof(AOEFFDRelEnt) * tab->relCount; // relEntries
	}

	return totalSize;
}

static uint32_t getEntrySymbolAddress(SymbolTable* symbTable, Config* config) {
	if (config->isDynamic) return 0x00000000; // Dynamic libraries do not have entry points

	char* entrySymbolName = NULL;
	if (config->isKernel) {
		entrySymbolName = "__onStart";
	} else if (config->useStdLib) entrySymbolName = "main";
	else entrySymbolName = "_init";

	int entrySymbIndex = getSymbolByName(symbTable, entrySymbolName, 0);
	if (entrySymbIndex == -1) emitError(ERR_UNDEFINED, "Entry point symbol %s is undefined", entrySymbolName);

	AOEFFSymEnt* entrySymb = &symbTable->symbols[entrySymbIndex];

	return entrySymb->seSymbVal;
}

static AOEFFSectHdr* normalizeSectionHeaders(SectionTable* sectTable, JumpTables* jumpTables, uint32_t sectOff) {
	// Updates the offset fields of the section headers to reflect their actual offsets in the final binary
	// Also adds a blank section header at the end

	int sectCount = 0;
	for (int i = 0; i < 5; i++) {
		if (sectTable->sections[i].shSectSize != 0) sectCount++;
	}
	if (jumpTables->fjt.fjtEntryCount != 0) sectCount++; // For .text.fjt
	if (jumpTables->djt.djtEntryCount != 0) sectCount++; // For .data.djt
	sectCount++; // For the blank ending entry

	AOEFFSectHdr* newSectHeaders = (AOEFFSectHdr*) calloc(sectCount, sizeof(AOEFFSectHdr));
	if (!newSectHeaders) emitError(ERR_MEM, "Failed to allocate memory for normalized section headers.");

	// Offset where all sections start at, basically the end of the dynamic relocation tables
	uint32_t baseOffset = sectOff;
	rlog("Base section for all section data: 0x%x", baseOffset);
	uint32_t sectOffset = baseOffset;
	int hdrIdx = 0;
	for (int i = 0; i < 5; i++) {
		if (sectTable->sections[i].shSectSize == 0) continue;

		newSectHeaders[hdrIdx] = sectTable->sections[i];
		newSectHeaders[hdrIdx].shSectOff = sectOffset;

		if (i == 2) newSectHeaders[hdrIdx].shSectOff = 0; // .bss has no offset

		log("Section %s: Offset 0x%x, Size 0x%x", newSectHeaders[hdrIdx].shSectName, newSectHeaders[hdrIdx].shSectOff, newSectHeaders[hdrIdx].shSectSize);

		if (i != 2) sectOffset += newSectHeaders[hdrIdx].shSectSize;
		hdrIdx++;
	}

	// Add .text.fjt section if there are function jump table entries
	if (jumpTables->fjt.fjtEntryCount != 0) {
		AOEFFSectHdr* fjtHdr = &newSectHeaders[hdrIdx];
		memcpy(fjtHdr->shSectName, ".fjt", 5);
		fjtHdr->shSectOff = sectOffset;
		fjtHdr->shSectSize = jumpTables->fjt.fjtEntryCount * (7 * 4); // Each entry is 7 instructions of 4 bytes each

		log("Section %s: Offset 0x%x, Size 0x%x", fjtHdr->shSectName, fjtHdr->shSectOff, fjtHdr->shSectSize);

		sectOffset += fjtHdr->shSectSize;
		hdrIdx++;
	}

	// Add .data.djt section if there are data jump table entries
	if (jumpTables->djt.djtEntryCount != 0) {
		AOEFFSectHdr* djtHdr = &newSectHeaders[hdrIdx];
		memcpy(djtHdr->shSectName, ".djt", 8);
		djtHdr->shSectOff = sectOffset;
		djtHdr->shSectSize = jumpTables->djt.djtEntryCount * 4; // Each entry is 4 bytes

		log("Section %s: Offset 0x%x, Size 0x%x", djtHdr->shSectName, djtHdr->shSectOff, djtHdr->shSectSize);

		sectOffset += djtHdr->shSectSize;
		hdrIdx++;
	}

	return newSectHeaders;
}

static AOEFFSymEnt* normalizeSymbolTable(SymbolTable* symbTable) {
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

static AOEFFDyLibEnt* createDynamicLibraryTable(DynamicLibraries* dyLibTable, AOEFFDyStrTab* dyLibStrTab, uint32_t* outStrTabSize) {
	AOEFFDyLibEnt* dylibEntries = (AOEFFDyLibEnt*) calloc(dyLibTable->count, sizeof(AOEFFDyLibEnt));
	if (!dylibEntries) emitError(ERR_MEM, NULL, "Failed to allocate memory for dynamic library table.");

	uint32_t strbCount = 0;
	for (uint32_t i = 0; i < dyLibTable->count; i++) {
		DyLibEntry* libEntry = &dyLibTable->libs[i];
		AOEFFDyLibEnt* dylibEntry = &dylibEntries[i];

		// Add library name to string table
		uint32_t nameIdx = strbCount;
		size_t nameLen = strlen(libEntry->dlName) + 1; // +1 for null-terminator

		dyLibStrTab->dlstStrs = (char*) realloc(dyLibStrTab->dlstStrs, strbCount + nameLen);
		if (!dyLibStrTab->dlstStrs) emitError(ERR_MEM, NULL, "Failed to allocate memory for dynamic library string table.");

		memcpy(&dyLibStrTab->dlstStrs[strbCount], libEntry->dlName, nameLen);
		strbCount += nameLen;

		dylibEntry->dlName = nameIdx;
		dylibEntry->dlVersion = 0b000000000000; // For now, versioning is not implemented
	}

	*outStrTabSize = strbCount;

	return dylibEntries;
}

void writeBinary(Config* config, GlobalTables* globalTables, struct ImportsExports* importsExports, JumpTables* jumpTables, DynamicLibraries* dyLibTable) {
	initScope("writeBinary");

	// The outfile may contain directories
	// Make sure they exist, if they don't, create them
	if (config->outfile) {
		char* dirpath = strdup(config->outfile);
		if (!dirpath) emitError(ERR_MEM, "Failed to allocate memory while preparing output path.");

		char* lastSlash = strrchr(dirpath, '/');
		if (lastSlash) {
			*lastSlash = '\0';

			// Create each intermediate directory
			for (char* p = dirpath + 1; *p; p++) {
				if (*p == '/') {
					*p = '\0';
					if (mkdir(dirpath, 0755) != 0 && errno != EEXIST) emitError(ERR_IO, "Failed to create directory %s: %s", dirpath, strerror(errno));
					*p = '/';
				}
			}

			// Create the final directory
			if (mkdir(dirpath, 0755) != 0 && errno != EEXIST) emitError(ERR_IO, "Failed to create directory %s: %s", dirpath, strerror(errno));
		}
		free(dirpath);
	}

	FILE* outfile = fopen(config->outfile, "wb");
	if (!outfile) emitError(ERR_IO, "Failed to open output file %s for writing.", config->outfile);


	int sectEntries = 0;
	for (int i = 0; i < 5; i++) {
		if (globalTables->sectionTable->sections[i].shSectSize != 0) sectEntries++;
	}
	if (jumpTables->fjt.fjtEntryCount != 0) sectEntries++; // For .text.fjt
	if (jumpTables->djt.djtEntryCount != 0) sectEntries++; // For .data.djt
	sectEntries++; // Ending blank entry

	uint32_t symbTableSize = globalTables->symbolTable->count;
	symbTableSize++; // Ending blank entry
	uint32_t symbOff = sizeof(AOEFFhdr) + (sizeof(AOEFFSectHdr) * sectEntries);

	uint32_t strTabOff = symbOff + (sizeof(AOEFFSymEnt) * symbTableSize);
	uint32_t strTabSize = globalTables->symbolTable->SymbolStringTable.strbCount;
	strTabSize += 16; // Ending string is 16 bytes

	uint32_t relStrOff = strTabOff + strTabSize;
	uint32_t relStrSize = globalTables->relocTable->RelocStringTable.strbCount;

	// uint32_t trelTabCount = globalTables->relocTable->trelocs.count;
	// uint32_t trelTabOff = relStrOff + relStrSize;
	// uint32_t trelTabSize = getTRelTabSize(globalTables->relocTable->trelocs.tables, trelTabCount);
	uint32_t trelTabCount = 0;
	uint32_t trelTabOff = 0;
	uint32_t trelTabSize = 0;

	uint32_t drelTabCount = globalTables->relocTable->drelocs.count;
	uint32_t drelTabOff = relStrOff + relStrSize;
	uint32_t drelTabSize = getDRelTabSize(globalTables->relocTable->drelocs.tables, drelTabCount);
	
	rlog("trelTabOff: 0x%x; trelTabSize: 0x%x", trelTabOff, trelTabSize);
	rlog("drelTabOff: 0x%x; drelTabSize: 0x%x", drelTabOff, drelTabSize);

	AOEFFDyStrTab dyLibStrTab = {
		.dlstStrs = NULL
	};
	uint32_t dyLibStrTabSize = 0;
	AOEFFDyLibEnt* dylibTable = createDynamicLibraryTable(dyLibTable, &dyLibStrTab, &dyLibStrTabSize);
	uint32_t dylibTableOff = drelTabOff + drelTabSize;
	uint32_t dylibTableSize = dyLibTable->count;
	uint32_t dyLibStrTabOff = dylibTableOff + (dylibTableSize * sizeof(AOEFFDyLibEnt));

	uint32_t importTableOff = dyLibStrTabOff + dyLibStrTabSize;
	uint32_t importTableSize = importsExports->Imports.count;

	uint32_t exportTableOff = importTableOff + (importTableSize  * sizeof(AOEFFImportEnt));
	uint32_t exportTableSize = 0;
	if (config->isDynamic) exportTableSize = importsExports->Exports.count;


	// Write header info
	AOEFFhdr header = {
		.hID = {AH_ID0, AH_ID1, AH_ID2, AH_ID3},
		.hType = config->isDynamic ? AHT_DLIB : (config->isKernel ? AHT_KERN : AHT_EXEC),
		.hEntry = getEntrySymbolAddress(globalTables->symbolTable, config),
		.hSectOff = sizeof(AOEFFhdr),
		.hSectSize = sectEntries-1,
		.hSymbOff = (symbTableSize > 1) ? symbOff : 0x0,
		.hSymbSize = symbTableSize-1,
		.hStrTabOff = (strTabSize > 0) ? strTabOff : 0x0,
		.hStrTabSize = strTabSize,
		.hRelStrTabOff = (relStrSize > 0) ? relStrOff : 0x0,
		.hRelStrTabSize = relStrSize,
		.hTRelTabOff = 0,
		.hTRelTabSize = 0,
		.hDRelTabOff = drelTabOff,
		.hDRelTabSize = drelTabCount,
		.hDyLibTabOff = (dylibTableSize > 0) ? dylibTableOff : 0x0,
		.hDyLibTabSize = dylibTableSize,
		.hDyLibStrTabOff = (dyLibStrTabSize > 0) ? dyLibStrTabOff : 0x0,
		.hDyLibStrTabSize = dyLibStrTabSize,
		.hImportTabOff = (importTableSize > 0) ? importTableOff : 0x0,
		.hImportTabSize = importTableSize,
		.hExportTabOff = (exportTableSize > 0) ? exportTableOff : 0x0,
		.hExportTabSize = exportTableSize
	};
	fwrite(&header, sizeof(AOEFFhdr), 1, outfile);

	// Write section headers
	AOEFFSectHdr* sectHeaders = normalizeSectionHeaders(globalTables->sectionTable, jumpTables, exportTableOff + exportTableSize);
	fwrite(sectHeaders, sizeof(AOEFFSectHdr), sectEntries, outfile);

	// Write symbol table
	AOEFFSymEnt* symbEntries = normalizeSymbolTable(globalTables->symbolTable);
	fwrite(symbEntries, sizeof(AOEFFSymEnt), symbTableSize, outfile);
	free(symbEntries);

	// Write string table
	fwrite(globalTables->symbolTable->SymbolStringTable.strTab.stStrs, sizeof(char), strTabSize, outfile);

	// Write relocation string table
	fwrite(globalTables->relocTable->RelocStringTable.strTab.rstStrs, sizeof(char), relStrSize, outfile);

	uint8_t zeroBufferPadding[4] = {0};

	// Write dynamic relocation tables
	for (uint32_t i = 0; i < drelTabCount; i++) {
		AOEFFDRelTab* tab = &globalTables->relocTable->drelocs.tables[i];
		rtrace("Writing dynamic relocation table %d at offset 0x%x", i, ftell(outfile));
		rlog("Writing DReloc Table %d: relSect=0x%x, relTabName=0x%x, relCount=%d", i, tab->relSect, tab->relTabName, tab->relCount);
		fwrite(&tab->relSect, sizeof(uint8_t), 1, outfile);
		fwrite(zeroBufferPadding, sizeof(uint8_t), 3, outfile); // padding
		fwrite(&tab->relTabName, sizeof(uint32_t), 1, outfile);
		fwrite(&tab->relCount, sizeof(uint32_t), 1, outfile);
		fwrite(zeroBufferPadding, sizeof(uint8_t), 4, outfile); // padding
		rtrace("Writing %d dynamic relocation entries at offset 0x%x", tab->relCount, ftell(outfile));
		fwrite(tab->relEntries, sizeof(AOEFFDRelEnt), tab->relCount, outfile);
	}

	// Write dynamic library table
	rtrace("Writing dynamic library table at offset 0x%x", ftell(outfile));
	for (uint32_t i = 0; i < dyLibTable->count; i++) {
		AOEFFDyLibEnt* dylibEntry = &dylibTable[i];
		rlog("Writing Dynamic Library Entry %d: nameIndex=%d, name=%s", i, dylibEntry->dlName, &dyLibStrTab.dlstStrs[dylibEntry->dlName]);
		fwrite(dylibEntry, sizeof(AOEFFDyLibEnt), 1, outfile);
	}

	// Write dynamic library string table
	rtrace("Writing dynamic library string table at offset 0x%x", ftell(outfile));
	fwrite(dyLibStrTab.dlstStrs, sizeof(char), dyLibStrTabSize, outfile);

	// Write import table
	rtrace("Writing import table at offset 0x%x", ftell(outfile));
	for (uint32_t i = 0; i < importsExports->Imports.count; i++) {
		AOEFFImportEnt* importEntry = &importsExports->Imports.entries[i];
		rlog("Writing Import Entry %d: symbIndex=%d, symbName=%s", i, importEntry->ieSymb, &globalTables->symbolTable->SymbolStringTable.strTab.stStrs[importEntry->ieSymb]);
		fwrite(importEntry, sizeof(AOEFFImportEnt), 1, outfile);
	}

	// Write export table
	for (uint32_t i = 0; i < importsExports->Exports.count; i++) {
		AOEFFExportEnt* exportEntry = &importsExports->Exports.entries[i];
		rlog("Writing Export Entry %d: symbIndex=%d, symbName=%s", i, exportEntry->eeSymb, &globalTables->symbolTable->SymbolStringTable.strTab.stStrs[exportEntry->eeSymb]);
		fwrite(exportEntry, sizeof(AOEFFExportEnt), 1, outfile);
	}

	// Write payload
	for (int i = 0; i < sectEntries; i++) {
		AOEFFSectHdr sectHdr = sectHeaders[i];
		if (sectHdr.shSectSize == 0 || sectHdr.shSectName[1] == 'b') continue;

		rlog("Writing section %s at offset 0x%x, size 0x%x", sectHdr.shSectName, sectHdr.shSectOff, sectHdr.shSectSize);
		rtrace("Writing at file offset 0x%x", ftell(outfile));
		if (strcmp(sectHdr.shSectName, ".data") == 0) {
			fwrite(globalTables->sectionTable->_data, sizeof(uint8_t), sectHdr.shSectSize, outfile);
		} else if (strcmp(sectHdr.shSectName, ".const") == 0) {
			fwrite(globalTables->sectionTable->_const, sizeof(uint8_t), sectHdr.shSectSize, outfile);
		} else if (strcmp(sectHdr.shSectName, ".text") == 0) {
			fwrite(globalTables->sectionTable->_text, sizeof(uint32_t), sectHdr.shSectSize / 4, outfile);
		} else if (strcmp(sectHdr.shSectName, ".evt") == 0) {
			fwrite(globalTables->sectionTable->_evt, sizeof(uint8_t), sectHdr.shSectSize, outfile);
		} else if (strcmp(sectHdr.shSectName, ".fjt") == 0) {
			fwrite(jumpTables->fjt._fjt, sizeof(uint32_t), jumpTables->fjt.fjtEntryCount * 7, outfile);
		} else if (strcmp(sectHdr.shSectName, ".djt") == 0) {
			fwrite(jumpTables->djt._djt, sizeof(uint32_t), jumpTables->djt.djtEntryCount, outfile);
		} else {
			emitError(ERR_INTERNAL, "Writing section %s is not implemented yet.", sectHdr.shSectName);
		}
	}
	rtrace("Ended file writing at 0x%x", ftell(outfile));
	free(sectHeaders);

	fclose(outfile);
}