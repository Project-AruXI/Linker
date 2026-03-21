#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "dylib.h"
#include "diagnostics.h"



static void* openLibrary(const char* libPath, size_t* fileSize) {
	int fd = open(libPath, O_RDONLY);
	if (fd < 0) emitError(ERR_IO, "Failed to open object file: %s", libPath);

	struct stat statBuffer;
	int rc = fstat(fd, &statBuffer);
	if (rc != 0) {
		write(STDERR_FILENO, "Failed to stat object file\n", 28);
		emitError(ERR_IO, "Failed to stat object file: %s", libPath);
	}

	void* ptr = mmap(0, statBuffer.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (ptr == MAP_FAILED) {
		write(STDERR_FILENO, "Failed to mmap object file\n", 28);
		emitError(ERR_IO, "Failed to mmap object file: %s", libPath);
	}
	close(fd);

	*fileSize = statBuffer.st_size;

	return ptr;
}


DynamicLibraries* verifyLibraries(Config* config) {
	DynamicLibraries* dyLibTable = (DynamicLibraries*) malloc(sizeof(DynamicLibraries));
	if (!dyLibTable) emitError(ERR_MEM, "Failed to allocate memory for dynamic library table");

	char* missingLibs[MAX_LIBS];
	uint32_t missingLibCount = 0;

	uint32_t i = 0;
	if (config->noStdLib || config->isKernel) i = 1; // First library is always stdlib

	for (; config->libs[i]; i++) {
		const char* libName = config->libs[i];
		bool found = false;

		for (uint32_t j = 0; config->libpath[j]; j++) {
			const char* searchPath = config->libpath[j];

			char fullPath[512];
			snprintf(fullPath, sizeof(fullPath), "%s/%s.adlib", searchPath, libName);

			FILE* file = fopen(fullPath, "rb");
			if (file) {
				// File exists, read export symbols
				fclose(file);
				log("Found dynamic library: %s", fullPath);
				found = true;

				size_t libFileSize = 0;
				void* lib = openLibrary(fullPath, &libFileSize);

				AOEFFhdr* libHeader = (AOEFFhdr*) lib;
				if (libHeader->hID[AHID_0] != AH_ID0 || libHeader->hID[AHID_1] != AH_ID1 ||
					libHeader->hID[AHID_2] != AH_ID2 || libHeader->hID[AHID_3] != AH_ID3) {
					emitError(ERR_INVALID_FORMAT, "Dynamic library %s is not a valid AOEFF file", fullPath);
				}

				if (libHeader->hType != AHT_DLIB) {
					emitError(ERR_INVALID_FORMAT, "Dynamic library %s is not a valid dynamic library", fullPath);
				}

				AOEFFExportEnt* exportEntries = (AOEFFExportEnt*) ((uint8_t*) lib + libHeader->hExportTabOff);

				AOEFFSymEnt* symbolTable = (AOEFFSymEnt*) ((uint8_t*) lib + libHeader->hSymbOff);
				char* strTab = (char*) lib + libHeader->hStrTabOff;

				DyLibEntry* dyLibEntry = &dyLibTable->libs[dyLibTable->count++];
				dyLibEntry->dlName = strdup((char*) libName);
				dyLibEntry->dlExportCount = libHeader->hExportTabSize;
				dyLibEntry->dlExportCapacity = libHeader->hExportTabSize;
				dyLibEntry->dlExportSymbs = (char**) malloc(sizeof(char*) * libHeader->hExportTabSize);
				if (!dyLibEntry->dlExportSymbs) emitError(ERR_MEM, "Failed to allocate memory for dynamic library export symbols");

				for (uint32_t k = 0; k < libHeader->hExportTabSize; k++) {
					AOEFFExportEnt* exportEntry = &exportEntries[k];
					uint32_t symbIndex = exportEntry->eeSymb;
					AOEFFSymEnt* symbEntry = &symbolTable[symbIndex];
					char* exportSymName = &strTab[symbEntry->seSymbName];

					dyLibEntry->dlExportSymbs[k] = strdup(exportSymName);

					log("Dynamic library %s exports symbol: %s", libName, exportSymName);
				}

				munmap(lib, libFileSize);
				break;
			}
		}

		if (!found) {
			missingLibs[missingLibCount++] = (char*) libName;
		}
	}

	if (missingLibCount > 0) {
		for (uint32_t i = 0; i < missingLibCount; i++) {
			emitError(ERR_MISSING_LIB, "Dynamic library not found: %s", missingLibs[i]);
		}
	}

	return dyLibTable;
}

void displayDynamicLibraries(DynamicLibraries* dyLibTable) {
	rtrace("== Dynamic Libraries ==");
	for (uint32_t i = 0; i < dyLibTable->count; i++) {
		DyLibEntry* dyLibEntry = &dyLibTable->libs[i];
		rtrace("----------------------");
		rtrace("DyLib: %s", dyLibEntry->dlName);
		rtrace("----------------------");
		for (uint32_t j = 0; j < dyLibEntry->dlExportCount; j++) {
			rtrace("[%d]  %s", j, dyLibEntry->dlExportSymbs[j]);
		}
		rtrace("----------------------\n");
	}
}


void initImports(struct ImportsExports* importsExports) {
	AOEFFImportEnt* importEntries = (AOEFFImportEnt*) malloc(sizeof(AOEFFImportEnt) * 4);
	if (!importEntries) emitError(ERR_MEM, "Failed to allocate memory for import symbols.");

	importsExports->Imports.entries = importEntries;
	importsExports->Imports.count = 0;
	importsExports->Imports.capacity = 4;
}
void deinitImports(struct ImportsExports* importsExports) {}


void importSymbols(SymbolTable* symbTable, RelocTable* relocTable, DynamicLibraries* dyLibTable, struct ImportsExports* importsExports) {
	initScope("importSymbols");

	log("Importing symbols from dynamic libraries...\n");

	// For each unresolved relocation, get the symbol from the symbol table
	// Check if the symbol is defined in any of the dynamic libraries
	// If so, add it to the import table
	// If not, emit an error that the symbol is undefined
	for (int i = 0; i < relocTable->unresolved.count; i++) {
		int* indexPair = relocTable->unresolved.unresolvedIndices[i];
		int relocTableIndex = indexPair[0];
		int relocEntryIndex = indexPair[1];

		AOEFFTRelTab* relTab = &relocTable->trelocs.tables[relocTableIndex];
		AOEFFTRelEnt* relEntry = &relTab->relEntries[relocEntryIndex];

		uint8_t symbIndex = relEntry->reSymb;
		AOEFFSymEnt* symbEntry = &symbTable->symbols[symbIndex];
		char* symbName = &symbTable->SymbolStringTable.strTab.stStrs[symbEntry->seSymbName];
		detail("Unresolved symbol from relocation: %s (Symb Index: %d)", symbName, symbIndex);

		bool found = false;
		for (uint32_t j = 0; j < dyLibTable->count; j++) {
			DyLibEntry* dyLibEntry = &dyLibTable->libs[j];
			for (uint32_t k = 0; k < dyLibEntry->dlExportCount; k++) {
				if (strcmp(symbName, dyLibEntry->dlExportSymbs[k]) == 0) {
					// Symbol found in dynamic library, add to imports
					found = true;
					log("Importing symbol %s from dynamic library %s", symbName, dyLibEntry->dlName);

					// Tag the type of symbol
					// In the proper world, the type would be determined straight from the dynamic library's symbol table
					// This would require DyLibEntry to change to also store the symbol types alongside the names, will do later
					// For now, assume programmer is right and assign the type based on the section where the relocation is at
					if (relTab->relSect == SECT_TEXT) {
						symbEntry->seSymbInfo = SE_SET_INFO(SE_FUNC_T, SE_GLOBL);
					} else if (relTab->relSect == SECT_DATA || relTab->relSect == SECT_CONST) {
						symbEntry->seSymbInfo = SE_SET_INFO(SE_OBJ_T, SE_GLOBL);
					} else {
						symbEntry->seSymbInfo = SE_SET_INFO(SE_NONE_T, SE_GLOBL);
					}

					// Add to importsExports
					if (importsExports->Imports.count == importsExports->Imports.capacity) {
						importsExports->Imports.capacity *= 2;
						AOEFFImportEnt* newEntries = (AOEFFImportEnt*) realloc(importsExports->Imports.entries, sizeof(AOEFFImportEnt) * importsExports->Imports.capacity);
						if (!newEntries) emitError(ERR_MEM, "Failed to reallocate memory for import entries");

						importsExports->Imports.entries = newEntries;
					}

					AOEFFImportEnt* importEntry = &importsExports->Imports.entries[importsExports->Imports.count++];
					importEntry->ieSymb = symbIndex;
					importEntry->ieDyLib = j; // Index of the dynamic library

					break;
				}
			}
		}

		if (!found) emitError(ERR_UNDEFINED, "Undefined symbol: %s", symbName);
	}
}

void displayImports(struct ImportsExports* importsExports) {}