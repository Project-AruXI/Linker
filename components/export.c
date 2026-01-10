#include <stdlib.h>

#include "dylib.h"
#include "diagnostics.h"


void initExports(struct ImportsExports* importsExports) {
	AOEFFExportEnt* exportEntries = (AOEFFExportEnt*) malloc(sizeof(AOEFFExportEnt) * 4);
	if (!exportEntries) emitError(ERR_MEM, "Failed to allocate memory for export symbols.");
	importsExports->Exports.entries = exportEntries;
	importsExports->Exports.count = 0;
	importsExports->Exports.capacity = 4;
}

void deinitExports(struct ImportsExports* importsExports) {

}

void exportSymbols(SymbolTable* symbTable, struct ImportsExports* importsExports) {
	// For now, only symbols marked as global are exported
	initScope("exportSymbols");

	for (uint32_t i = 0; i < symbTable->count; i++) {
		AOEFFSymEnt* symbEntry = &symbTable->symbols[i];

		uint8_t symbLoc = SE_GET_LOC(symbEntry->seSymbInfo);
		uint8_t symbType = SE_GET_TYPE(symbEntry->seSymbInfo);
		uint32_t symbSect = symbEntry->seSymbSect;

		// Only global defined symbols are exported
		if (symbLoc == SE_GLOBL && symbType != SE_NONE_T && symbSect != SE_SECT_UNDEF) {
			// Export this symbol
			if (importsExports->Exports.count == importsExports->Exports.capacity) {
				uint32_t newCap = importsExports->Exports.capacity * 2;
				AOEFFExportEnt* newEntries = (AOEFFExportEnt*) realloc(importsExports->Exports.entries, sizeof(AOEFFExportEnt) * newCap);
				if (!newEntries) emitError(ERR_MEM, "Failed to allocate memory for export symbols.");

				importsExports->Exports.entries = newEntries;
				importsExports->Exports.capacity = newCap;
			}

			AOEFFExportEnt* exportEntry = &importsExports->Exports.entries[importsExports->Exports.count];
			exportEntry->eeSymb = i;
			exportEntry->eeAddress = symbEntry->seSymbVal;

			importsExports->Exports.count++;

			log("Exported symbol index %d", i);
			log("Exported symbol address 0x%X", symbEntry->seSymbVal);
		}
	}
}

void displayExports(struct ImportsExports* importsExports) {}