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
	uint32_t strTabSize = objHeader->hStrTabSize;
	uint32_t relStrTabSize = objHeader->hRelStrTabSize;

	// SectionTable* localSectTable = initSectionTable();

	FileCtx filectx = {
		.filename = strdup(infile),
		.textOffset = 0x0,
		.dataOffset = 0x0,
		.constOffset = 0x0
	};

	// Get the sections themselves
	for (uint32_t i = 0; i < objHeader->hSectSize-1; i++) {
		AOEFFSectHdr* sectHdr = &sectHeaders[i];
		log("Section %d: name=%.8s, off=0x%X, size=0x%X", i, sectHdr->shSectName, sectHdr->shSectOff, sectHdr->shSectSize);

		uint32_t sectionOffset = sectHdr->shSectOff;
		uint32_t sectionSize = sectHdr->shSectSize;

		// TODO: Refactor to be cleaner
		if (sectHdr->shSectName[1] == 't') { // .text
			uint32_t* aobjText = (uint32_t*) ((uint8_t*) _obj + sectionOffset);
			uint32_t aobjTextSize = sectionSize;

			appendSection(globalTables->sectionTable, sectHdr, aobjText);

			// As per the comment, the size of the global text section is the last file context's textOffset + size of the section from section header
			uint32_t prevFilectxTextOffset = 0;
			if (globalTables->sectionTable->filectxs.count > 0) {
				FileCtx* prevFilectx = &globalTables->sectionTable->filectxs.ctx[globalTables->sectionTable->filectxs.count - 1];
				prevFilectxTextOffset = prevFilectx->textOffset;
			}
			log("Previous file context text offset: 0x%X", prevFilectxTextOffset);
			uint32_t globalTextSize = prevFilectxTextOffset + sectHdr->shSectSize;
			// The new file context's text offset is the previous global text size
			filectx.textOffset = globalTextSize;
			log("Updated file context text offset to 0x%X", filectx.textOffset);

			uint32_t* newGlobalText = NULL;
			if (!globalTables->sectionTable->_text) {
				trace("Allocating global text section for the first time");
				// First allocation
				newGlobalText = (uint32_t*) malloc(globalTextSize);
				if (!newGlobalText) emitError(ERR_MEM, "Failed to allocate memory for global text section");
			} else {
				newGlobalText = (uint32_t*) realloc(globalTables->sectionTable->_text, globalTextSize);
				if (!newGlobalText) emitError(ERR_MEM, "Failed to reallocate memory for global text section");
			}
			globalTables->sectionTable->_text = newGlobalText;
			// Copy over the new text section data
			memcpy((uint8_t*) globalTables->sectionTable->_text + prevFilectxTextOffset, aobjText, aobjTextSize);
		} else if (sectHdr->shSectName[1] == 'd') { // .data
			uint8_t* aobjData = (uint8_t*) _obj + sectionOffset;
			uint32_t aobjDataSize = sectionSize;

			appendSection(globalTables->sectionTable, sectHdr, aobjData);

			// As per the comment, the size of the global data section is the last file context's dataOffset + size of the section from section header
			uint32_t prevFilectxDataOffset = 0;
			if (globalTables->sectionTable->filectxs.count > 0) {
				FileCtx* prevFilectx = &globalTables->sectionTable->filectxs.ctx[globalTables->sectionTable->filectxs.count - 1];
				prevFilectxDataOffset = prevFilectx->dataOffset;
			}
			log("Previous file context data offset: 0x%X", prevFilectxDataOffset);
			uint32_t globalDataSize = prevFilectxDataOffset + sectHdr->shSectSize;
			// The new file context's data offset is the previous global data size
			filectx.dataOffset = globalDataSize;
			log("Updated file context data offset to 0x%X", filectx.dataOffset);

			uint8_t* newGlobalData = NULL;
			if (!globalTables->sectionTable->_data) {
				trace("Allocating global data section for the first time");
				// First allocation
				newGlobalData = (uint8_t*) malloc(globalDataSize);
				if (!newGlobalData) emitError(ERR_MEM, "Failed to allocate memory for global data section");
			} else {
				newGlobalData = (uint8_t*) realloc(globalTables->sectionTable->_data, globalDataSize);
				if (!newGlobalData) emitError(ERR_MEM, "Failed to reallocate memory for global data section");
			}
			globalTables->sectionTable->_data = newGlobalData;
			// Copy over the new data section data
			memcpy((uint8_t*) globalTables->sectionTable->_data + prevFilectxDataOffset, aobjData, aobjDataSize);
		} else if (sectHdr->shSectName[1] == 'c') { // .const
			uint8_t* aobjConst = (uint8_t*) _obj + sectionOffset;
			uint32_t aobjConstSize = sectionSize;

			appendSection(globalTables->sectionTable, sectHdr, aobjConst);

			// As per the comment, the size of the global const section is the last file context's constOffset + size of the section from section header
			uint32_t prevFilectxConstOffset = 0;
			if (globalTables->sectionTable->filectxs.count > 0) {
				FileCtx* prevFilectx = &globalTables->sectionTable->filectxs.ctx[globalTables->sectionTable->filectxs.count - 1];
				prevFilectxConstOffset = prevFilectx->constOffset;
			}
			log("Previous file context const offset: 0x%X", prevFilectxConstOffset);
			uint32_t globalConstSize = prevFilectxConstOffset + sectHdr->shSectSize;
			// The new file context's const offset is the previous global const size
			filectx.constOffset = globalConstSize;
			log("Updated file context const offset to 0x%X", filectx.constOffset);

			uint8_t* newGlobalConst = NULL;
			if (!globalTables->sectionTable->_const) {
				trace("Allocating global const section for the first time");
				// First allocation
				newGlobalConst = (uint8_t*) malloc(globalConstSize);
				if (!newGlobalConst) emitError(ERR_MEM, "Failed to allocate memory for global const section");
			} else {
				newGlobalConst = (uint8_t*) realloc(globalTables->sectionTable->_const, globalConstSize);
				if (!newGlobalConst) emitError(ERR_MEM, "Failed to reallocate memory for global const section");
			}
			globalTables->sectionTable->_const = newGlobalConst;
			// Copy over the new const section data
			memcpy((uint8_t*) globalTables->sectionTable->_const + prevFilectxConstOffset, aobjConst, aobjConstSize);
		} else {
			emitError(ERR_INVALID_FORMAT, "Unsupported section type in input file %s: %.8s", infile, sectHdr->shSectName);
		}
	}

	addFileContext(globalTables->sectionTable, filectx);

	// Add symbols to global symbol table
	for (uint32_t i = 0; i < objHeader->hSymbSize-1; i++) {
		AOEFFSymEnt* symbEnt = &symbEntries[i];

		// char* symName = &strTab.stStrs[symbEnt->seSymbName];
		char* symName = strTab.stStrs + symbEnt->seSymbName;

		log("Symbol %d: nameIdx=%d (%s), size=0x%X, val=0x%X, info=0x%X, sect=%d", i, symbEnt->seSymbName, symName, symbEnt->seSymbSize, symbEnt->seSymbVal, symbEnt->seSymbInfo, symbEnt->seSymbSect);
		appendSymbol(globalTables->symbolTable, *symbEnt);
	}






}