#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "merge.h"
#include "diagnostics.h"


static void* mapObjectFile(const char* path) {
	int fd = open(path, O_RDONLY);
	if (fd < 0) emitError(ERR_IO, "Failed to open object file: %s", path);

	struct stat statBuffer;
	int rc = fstat(fd, &statBuffer);
	if (rc != 0) {
		write(STDERR_FILENO, "Failed to stat object file\n", 28);
		emitError(ERR_IO, "Failed to stat object file: %s", path);
	}

	void* ptr = mmap(0, statBuffer.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (ptr == MAP_FAILED) {
		write(STDERR_FILENO, "Failed to mmap object file\n", 28);
		emitError(ERR_IO, "Failed to mmap object file: %s", path);
	}
	close(fd);

	return ptr;	
}


void updateSymbolAddressValue(AOEFFSymEnt* addedSymbol, const char* symName, FileCtx filectx, uint32_t memOffsets[5]) {
	// Get the section's global offset
	uint32_t sectionGlobalOffset = 0;
	if (addedSymbol->seSymbSect == 3) sectionGlobalOffset = filectx.textOffset + memOffsets[3];
	else if (addedSymbol->seSymbSect == 0) sectionGlobalOffset = filectx.dataOffset + memOffsets[0];
	else if (addedSymbol->seSymbSect == 1) sectionGlobalOffset = filectx.constOffset + memOffsets[1];
	else if (addedSymbol->seSymbSect == 2) sectionGlobalOffset = filectx.bssOffset + memOffsets[2]; // BSS section starts at its memory offset
	else if (addedSymbol->seSymbSect == 4) sectionGlobalOffset = 0x0; // Evt section fixed address
	else emitError(ERR_INTERNAL, "Unsupported section index %d for symbol %s while updating symbol value", addedSymbol->seSymbSect, symName);

	log("Updating symbol %s's value from 0x%X to 0x%X (section global offset 0x%X, original value 0x%X)", 
			symName, addedSymbol->seSymbVal, sectionGlobalOffset + addedSymbol->seSymbVal, sectionGlobalOffset, addedSymbol->seSymbVal);
	addedSymbol->seSymbVal = sectionGlobalOffset + addedSymbol->seSymbVal;
}


void merge(const char* infile, GlobalTables* globalTables) {
	initScope("merge");

	void* _obj = mapObjectFile(infile);

	AOEFFhdr* objHeader = (AOEFFhdr*) _obj;
	if (objHeader->hID[AHID_0] != AH_ID0 || objHeader->hID[AHID_1] != AH_ID1 ||
		objHeader->hID[AHID_2] != AH_ID2 || objHeader->hID[AHID_3] != AH_ID3) {
		emitError(ERR_INVALID_FORMAT, "Input file %s is not a valid AOEFF file", infile);
	}

	if (objHeader->hType != AHT_AOBJ && objHeader->hType != AHT_SLIB) emitError(ERR_INVALID_FORMAT, "Input file %s is not an object file or static library", infile);

	// Get the tables
	AOEFFSectHdr* sectHeaders = (AOEFFSectHdr*) (_obj + objHeader->hSectOff);
	AOEFFSymEnt* symbEntries = (AOEFFSymEnt*) (_obj + objHeader->hSymbOff);
	AOEFFStrTab strTab;
	strTab.stStrs = (char*) (_obj + objHeader->hStrTabOff);
	AOEFFTRelTab* tRelTables = (AOEFFTRelTab*) (_obj + objHeader->hTRelTabOff);
	AOEFFRelStrTab relStrTab;
	relStrTab.rstStrs = (char*) (_obj + objHeader->hRelStrTabOff);
	// uint32_t strTabSize = objHeader->hStrTabSize;
	// uint32_t relStrTabSize = objHeader->hRelStrTabSize;


	FileCtx filectx = {
		.filename = strdup(infile),
		.textOffset = 0x0,
		.dataOffset = 0x0,
		.constOffset = 0x0,
		.bssOffset = 0x0
	};

	// Get the sections themselves
	for (uint32_t i = 0; i < objHeader->hSectSize-1; i++) {
		AOEFFSectHdr* sectHdr = &sectHeaders[i];
		log("Section %d: name=%.8s, off=0x%X, size=0x%X", i, sectHdr->shSectName, sectHdr->shSectOff, sectHdr->shSectSize);

		uint32_t sectionOffset = sectHdr->shSectOff;
		uint32_t sectionSize = sectHdr->shSectSize;

		uint32_t prevSizeSect = 0; // The previous size of the global section being merged into

		// TODO: Refactor to be cleaner
		if (sectHdr->shSectName[1] == 't') { // .text
			uint32_t* aobjText = (uint32_t*) ((uint8_t*) _obj + sectionOffset);
			uint32_t aobjTextSize = sectionSize;

			prevSizeSect = globalTables->sectionTable->sections[3].shSectSize;

			appendSection(globalTables->sectionTable, sectHdr, aobjText);

			uint32_t newGlobalTextSize = prevSizeSect + sectHdr->shSectSize;
			// The new file context's text offset is the previous global text size
			filectx.textOffset = prevSizeSect;
			log("Updated file context text offset to 0x%X", filectx.textOffset);

			uint32_t* newGlobalText = NULL;
			if (!globalTables->sectionTable->_text) {
				trace("Allocating global text section for the first time");
				// First allocation
				newGlobalText = (uint32_t*) malloc(newGlobalTextSize);
				if (!newGlobalText) emitError(ERR_MEM, "Failed to allocate memory for global text section");
			} else {
				newGlobalText = (uint32_t*) realloc(globalTables->sectionTable->_text, newGlobalTextSize);
				if (!newGlobalText) emitError(ERR_MEM, "Failed to reallocate memory for global text section");
				detail("Reallocated to %p", newGlobalText);
			}
			globalTables->sectionTable->_text = newGlobalText;
			// Copy over the new text section data
			memcpy((uint8_t*) globalTables->sectionTable->_text + prevSizeSect, aobjText, aobjTextSize);
			detail("Copied text data to %p (prevSizeSect=0x%X)", (uint8_t*) globalTables->sectionTable->_text + prevSizeSect, prevSizeSect);
			detail("First instruction of text section to add: 0x%X", aobjText[0]);
			detail("%p", globalTables->sectionTable->_text);

		} else if (sectHdr->shSectName[1] == 'd') { // .data
			uint8_t* aobjData = (uint8_t*) _obj + sectionOffset;
			uint32_t aobjDataSize = sectionSize;

			prevSizeSect = globalTables->sectionTable->sections[0].shSectSize;

			appendSection(globalTables->sectionTable, sectHdr, aobjData);

			uint32_t newGlobalDataSize = prevSizeSect + sectHdr->shSectSize;
			// The new file context's data offset is the previous global data size
			filectx.dataOffset = prevSizeSect;
			log("Updated file context data offset to 0x%X", filectx.dataOffset);

			uint8_t* newGlobalData = NULL;
			if (!globalTables->sectionTable->_data) {
				trace("Allocating global data section for the first time");
				// First allocation
				newGlobalData = (uint8_t*) malloc(newGlobalDataSize);
				if (!newGlobalData) emitError(ERR_MEM, "Failed to allocate memory for global data section");
			} else {
				newGlobalData = (uint8_t*) realloc(globalTables->sectionTable->_data, newGlobalDataSize);
				if (!newGlobalData) emitError(ERR_MEM, "Failed to reallocate memory for global data section");
			}
			globalTables->sectionTable->_data = newGlobalData;
			// Copy over the new data section data
			memcpy((uint8_t*) globalTables->sectionTable->_data + prevSizeSect, aobjData, aobjDataSize);
		} else if (sectHdr->shSectName[1] == 'c') { // .const
			uint8_t* aobjConst = (uint8_t*) _obj + sectionOffset;
			uint32_t aobjConstSize = sectionSize;

			prevSizeSect = globalTables->sectionTable->sections[1].shSectSize;

			appendSection(globalTables->sectionTable, sectHdr, aobjConst);

			uint32_t newGlobalConstSize = prevSizeSect + sectHdr->shSectSize;
			// The new file context's const offset is the previous global const size
			filectx.constOffset = prevSizeSect;
			log("Updated file context const offset to 0x%X", filectx.constOffset);

			uint8_t* newGlobalConst = NULL;
			if (!globalTables->sectionTable->_const) {
				trace("Allocating global const section for the first time");
				// First allocation
				newGlobalConst = (uint8_t*) malloc(newGlobalConstSize);
				if (!newGlobalConst) emitError(ERR_MEM, "Failed to allocate memory for global const section");
			} else {
				newGlobalConst = (uint8_t*) realloc(globalTables->sectionTable->_const, newGlobalConstSize);
				if (!newGlobalConst) emitError(ERR_MEM, "Failed to reallocate memory for global const section");
			}
			globalTables->sectionTable->_const = newGlobalConst;
			// Copy over the new const section data
			memcpy((uint8_t*) globalTables->sectionTable->_const + prevSizeSect, aobjConst, aobjConstSize);
		} else if (sectHdr->shSectName[1] == 'b') { // .bss
			uint32_t aobjBssSize = sectionSize; // Just the size, it does not have any data present in binary

			// BSS section has no data to copy over
			prevSizeSect = globalTables->sectionTable->sections[2].shSectSize;

			appendSection(globalTables->sectionTable, sectHdr, NULL);

			uint32_t newGlobalBssSize = prevSizeSect + sectHdr->shSectSize;
			// The new file context's bss offset is the previous global bss size
			filectx.bssOffset = prevSizeSect;
			log("Updated file context bss offset to 0x%X", filectx.bssOffset);
		} else if (sectHdr->shSectName[1] == 'e') { // .evt
			uint8_t* aobjEvt = (uint8_t*) _obj + sectionOffset;
			uint32_t aobjEvtSize = sectionSize;

			// Evt section does not have per-file offsets since it's kernel-only
			// And since it can only exist once, the offset is always 0 and there is no "global" merging needed
			appendSection(globalTables->sectionTable, sectHdr, aobjEvt);

			uint8_t* evt = NULL;
			if (!globalTables->sectionTable->_evt) {
				trace("Allocating global evt section for the first time");
				// First allocation
				evt = (uint8_t*) malloc(aobjEvtSize);
				if (!evt) emitError(ERR_MEM, "Failed to allocate memory for evt section");
			} else {
				// This means it was seen again, error
				emitError(ERR_REDEFINED, "Evt section defined multiple times (in file %s)", infile);
			}
			globalTables->sectionTable->_evt = evt;
			// Copy over the new evt section data
			memcpy((uint8_t*) globalTables->sectionTable->_evt + prevSizeSect, aobjEvt, aobjEvtSize);
		} else {
			emitError(ERR_INVALID_FORMAT, "Unsupported section type in input file %s: %.8s", infile, sectHdr->shSectName);
		}
	}

	addFileContext(globalTables->sectionTable, filectx);

	// Add symbols to global symbol table
	// For now, skip non-global symbols
	// Also add strings to global string table

	// Relocation entries reference symbol indices, so we need to update those as well after adding symbols

	// Get the start index of the newly added symbols to aid with updating relocation entries
	// Specially, indicate where to start searching the symbol table in order to decrease search time
	int startIndexOfSymbols = globalTables->symbolTable->count;

	for (uint32_t i = 0; i < objHeader->hSymbSize-1; i++) {
		AOEFFSymEnt* symbEnt = &symbEntries[i];
		char* symName = &strTab.stStrs[symbEnt->seSymbName];

		int filectxIndex = globalTables->sectionTable->filectxs.count - 1;

		// If the symbol already exists:
		// - One could be an external, the other defined
		//   - If the existing one is external and the new one is defined, update the existing one to be defined
		//   - If the existing one is defined and the new one is external, skip
		// - Both defined, in which case it's an error
		// - Both external, skip; only one external symbol is needed
		// - If local:
		// 	 - Keep it in the table for now, all local symbols will be dropped after relocation
		// 	 - Correction: locals will be kept unless an option to remove them will be added

		int symbIndexInGlobal = getSymbolByName(globalTables->symbolTable, symName, 0);
		if (symbIndexInGlobal != -1) {
			// The symbol already exists in the global symbol table
			AOEFFSymEnt* existingSymEntry = &globalTables->symbolTable->symbols[symbIndexInGlobal];

			uint8_t existingLoc = SE_GET_LOC(existingSymEntry->seSymbInfo);
			uint8_t existingType = SE_GET_TYPE(existingSymEntry->seSymbInfo);
			uint32_t existingSect = existingSymEntry->seSymbSect;
			// Symbols that are marked as external are: global, type of none, sect of undefined

			uint8_t newLoc = SE_GET_LOC(symbEnt->seSymbInfo);
			uint8_t newType = SE_GET_TYPE(symbEnt->seSymbInfo);
			uint32_t newSect = symbEnt->seSymbSect;

			if (existingLoc == SE_GLOBL && existingType == SE_NONE_T && existingSect == SE_SECT_UNDEF) {
				trace("Existing symbol %s is external", symName);
				// Existing is external
				if (newLoc == SE_GLOBL && newType != SE_NONE_T && newSect != SE_SECT_UNDEF) {
					trace("New symbol %s is defined", symName);
					// New is defined
					log("Updating existing external symbol %s to be defined", symName);
					existingSymEntry->seSymbInfo = symbEnt->seSymbInfo;
					existingSymEntry->seSymbSect = symbEnt->seSymbSect;
					existingSymEntry->seSymbVal = symbEnt->seSymbVal;
					updateSymbolAddressValue(existingSymEntry, symName, filectx, globalTables->sectionTable->sectionOffsets);
					existingSymEntry->seSymbSize = symbEnt->seSymbSize;
					globalTables->symbolTable->filectxIndices[symbIndexInGlobal] = filectxIndex;
					continue;
				} else {
					trace("New symbol %s is also external", symName);
					// New is also external, skip
					log("Skipping addition of symbol %s as it already exists as an external symbol", symName);
					continue;
				}
			} else {
				trace("Existing symbol %s is defined", symName);
				// Existing is defined
				if (newLoc == SE_GLOBL && newType == SE_NONE_T && newSect == SE_SECT_UNDEF) {
					trace("New symbol %s is external", symName);
					// New is external, skip
					log("Skipping addition of symbol %s as it already exists as a defined symbol", symName);
					continue;
				} else if (existingLoc == SE_GLOBL && newLoc == SE_GLOBL) {
					trace("New symbol %s is defined", symName);
					// Both are defined, error
					emitError(ERR_REDEFINED, "Symbol %s is defined multiple times (in file %s)", symName, infile);
				} else if (existingLoc == SE_LOCAL || newLoc == SE_LOCAL) {
					// Note: there may be repetition of local absolute symbols with the same value
					//   ie local constants defined in multiple files via .adecl or by hand
					// For now, the repetition remains
					trace("At least one of the symbols %s is local", symName);
					// At least one is local, keep both for now
					log("Keeping local symbol %s in the global symbol table for now", symName);
					// Continue to add the new symbol below
				}
			}
		}


		log("Symbol %d: nameIdx=%d (%s), size=0x%X, val=0x%X, info=0x%X, sect=%d", i, symbEnt->seSymbName, symName, symbEnt->seSymbSize, symbEnt->seSymbVal, symbEnt->seSymbInfo, symbEnt->seSymbSect);
		AOEFFSymEnt* addedSymbol = appendSymbol(globalTables->symbolTable, *symbEnt); 
		uint32_t newIndex = appendString(globalTables->symbolTable, symName);
		// Update the symbol entry's name index to the new index in the global string table
		addedSymbol->seSymbName = newIndex;

		// In the case that the symbol is an address in a section, we need to update the symbol's value
		// The old value is just an offset in the (local) section
		// The new value is the global section's offset + the old value
		if (SE_GET_TYPE(addedSymbol->seSymbInfo) != SE_NONE_T || addedSymbol->seSymbSect != SE_SECT_UNDEF) {
			updateSymbolAddressValue(addedSymbol, symName, filectx, globalTables->sectionTable->sectionOffsets);

			globalTables->symbolTable->filectxIndices[globalTables->symbolTable->count - 1] = filectxIndex;
		} else {
			// Take the chance that at this point, the symbol is external
			// Meaning it has no "file context"
			globalTables->symbolTable->filectxIndices[globalTables->symbolTable->count - 1] = -1;
		}
	}

	uint32_t currTRelTabOffset = 0x0;
	for (uint32_t i = 0; i < objHeader->hTRelTabSize; i++) {
		uint8_t* temp = (uint8_t*) tRelTables;
		AOEFFTRelTab* trelTab = (AOEFFTRelTab*)(temp + currTRelTabOffset);
		currTRelTabOffset += (sizeof(AOEFFTRelTab) - 8) + (sizeof(AOEFFTRelEnt) * (trelTab->relCount));
		// The above is needed because the relocation entries are variable-length arrays at the end of the relocation table struct
		// Also, the entries start where, per the struct definition, relEntries is located at
		// Since `sizeof(AOEFFTRelTab)` includes the 8 bytes for the pointer to relEntries, we need to subtract that out and add the actual size of the entries

		char* relTabName = &relStrTab.rstStrs[trelTab->relTabName];

		log("Relocation Table %d: sect=%d, nameIdx=%d (%s), entryCount=%d", i, trelTab->relSect, trelTab->relTabName, relTabName, trelTab->relCount);

		appendTRelocTable(globalTables->relocTable, trelTab, globalTables->sectionTable->filectxs.count - 1);
		uint32_t newIndex = appendRelocString(globalTables->relocTable, relTabName);
		// Update the relocation table's name index to the new index in the global relocation string table
		globalTables->relocTable->trelocs.tables[globalTables->relocTable->trelocs.count - 1].relTabName = newIndex;


		// Reference to the comment earlier about symbol indices potentially being out of sync
		// The indices refer to the local symbol tables
		// Use the symbol names (from the local string table) to find the new indices in the global symbol table
		for (uint32_t j = 0; j < trelTab->relCount; j++) {
			AOEFFTRelEnt* trueEntries = (AOEFFTRelEnt*) &trelTab->relEntries;
			AOEFFTRelEnt* relEnt = &trueEntries[j];
			uint32_t symbIndex = relEnt->reSymb; // The old symbol index
			AOEFFSymEnt* localSymbEnt = &symbEntries[symbIndex];
			char* localSymbName = &strTab.stStrs[localSymbEnt->seSymbName];
			int globalSymbIndex = getSymbolByName(globalTables->symbolTable, localSymbName, 0);
			if (globalSymbIndex == -1) emitError(ERR_INTERNAL, "Failed to find symbol %s in global symbol table while updating relocation entries", localSymbName);

			log("Updating relocation entry %d's symbol index from %d (local) to %d (global) for symbol %s", j, symbIndex, globalSymbIndex, localSymbName);
			globalTables->relocTable->trelocs.tables[globalTables->relocTable->trelocs.count - 1].relEntries[j].reSymb = (uint8_t) globalSymbIndex;
		}
	}
}