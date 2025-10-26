#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "dylink.h"
#include "diagnostics.h"


static void* mapLibraryFile(const char* path) {
	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		// No need to error here as it only indicates that the library was not found in this path
		return NULL;
	}

	struct stat statBuffer;
	int rc = fstat(fd, &statBuffer);
	if (rc != 0) {
		write(STDERR_FILENO, "Failed to stat library file\n", 28);
		emitError(ERR_IO, "Failed to stat library file: %s", path);
	}

	void* ptr = mmap(0, statBuffer.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (ptr == MAP_FAILED) {
		write(STDERR_FILENO, "Failed to mmap library file\n", 29);
		emitError(ERR_IO, "Failed to mmap library file: %s", path);
	}
	close(fd);

	return ptr;	
}


DyLibTable* dynLibBuild(SymbolTable* globalSymTable, const char** libPath, char* libs[32]) {
	// For each unresolved symbol in the global symbol table
	// 	Open each linked library, search it in path
	// 	If found, open it; for each symbol in the lib, check if it matches the unresolved symbol
	// 		If so, add it to the dynamic library table and mark the symbol as resolved

	DyLibTable* dyLibTable = initDyLibTable();


	for (int i = 0; i < globalSymTable->unresolved.count; i++) {
		uint32_t globalSymbUnresolvedIndex = globalSymTable->unresolved.unresolvedIndices[i];
		AOEFFSymbEntry* globalUnresolvedSymb = &globalSymTable->symbols[globalSymbUnresolvedIndex];

		// Search for this symbol in the linked libraries
		for (int j = 0; libs[j]; j++) {
			const char* libName = libs[j];

			for (int k = 0; libPath[k]; k++) {
				char fullPath[512];
				snprintf(fullPath, sizeof(fullPath), "%s/%s.adlib", libPath[k], libName);

				void* _lib = mapLibraryFile(fullPath);
				if (!_lib) continue; // Library not found in this path, try next

				uint8_t* lib = (uint8_t*) _lib;
				AOEFFheader* header = (AOEFFheader*) lib;

				AOEFFSymbEntry* adlibSymbTable = (AOEFFSymbEntry*) (lib + header->hSymbOff);
				AOEFFStringTab* adlibStrTab = (AOEFFStringTab*) (lib + header->hStrTabOff);

				// Look for the unresolved symbol in this library's symbol table
				for (int m = 0; m < header->hSymbSize; m++) {
					AOEFFSymbEntry* libSymbEntry = &adlibSymbTable[m];

					// Compare symbol names
					uint32_t libSymbEntryStrIdx = libSymbEntry->seSymbName;
					uint32_t globSymbEntryStrIdx = globalUnresolvedSymb->seSymbName;
					const char* libSymbName = &adlibStrTab->stStrs[libSymbEntryStrIdx];
					const char* globSymbName = &globalSymTable->SymbolStringTable.strTab.stStrs[globSymbEntryStrIdx];

					if (strcmp(libSymbName, globSymbName) == 0) {
						DyLibTableEntry* dyLibEntry = addDyLibSymb(dyLibTable, libName, globalUnresolvedSymb, globSymbName);

						goto next;
					}
				}
			}
		}

		// Symbol not found in any linked library
		emitError(ERR_UNDEFINED, "Unresolved symbol '%s' not found in linked dynamic libraries", 
			&globalSymTable->SymbolStringTable.strTab.stStrs[globalUnresolvedSymb->seSymbName]);


		// This is for when the symbol was found and added, go to the next symbol
		// The continue should really be inside the comparison but since continue cannot be used for an outer loop
		// It must be circumvented with a goto
		next: continue;
	}

	return dyLibTable;
}