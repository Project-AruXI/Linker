#include <stdlib.h>

#include "SectionTable.h"
#include "diagnostics.h"

SectionTable* initSectionTable() {
	SectionTable* sectTable = (SectionTable*) malloc(sizeof(SectionTable));
	if (!sectTable) emitError(ERR_MEM, "Failed to allocate memory for section table");

	sectTable->sections = (AOEFFSectHdr*) malloc(sizeof(AOEFFSectHdr) * 10);
	if (!sectTable->sections) emitError(ERR_MEM, "Failed to allocate memory for section table entries");

	sectTable->count = 0;
	sectTable->cap = 10;

	sectTable->_data = NULL;
	sectTable->_const = NULL;
	sectTable->_text = NULL;

	return sectTable;
}

void deinitSectionTable(SectionTable* sectTable) {
}

void appendSection(SectionTable* sectTable, AOEFFSectHdr* sectHdr, void* sectionData) {
	if (sectTable->count == sectTable->cap) {
		sectTable->cap += 2;
		sectTable->sections = (AOEFFSectHdr*) realloc(sectTable->sections, sizeof(AOEFFSectHdr) * sectTable->cap);
		if (!sectTable->sections) emitError(ERR_MEM, "Failed to reallocate memory for section table entries");
	}
	sectTable->sections[sectTable->count++] = *sectHdr;
}

void addFileContext(SectionTable* sectTable, FileCtx filectx) {
	if (sectTable->filectxs.count == sectTable->filectxs.cap) {
		sectTable->filectxs.cap += 2;
		sectTable->filectxs.ctx = (FileCtx*) realloc(sectTable->filectxs.ctx, sizeof(FileCtx) * sectTable->filectxs.cap);
		if (!sectTable->filectxs.ctx) emitError(ERR_MEM, "Failed to reallocate memory for file contexts");
	}
	sectTable->filectxs.ctx[sectTable->filectxs.count++] = filectx;
}

void displaySectionTable(SectionTable* sectTable) {
	/**
	 * Display as:
	 * ===== Section Table =====
	 * Total sections: x
	 * --------------------------
	 * # |   Name   |  Offset  |  Size  |
	 * ------------------------------------
	 * x |  xxxx    | 0xXXXXXX | 0xXXXX |
	 * ------------------------------------
	 * ...
	 * ------------------------------------
	 * 
	 * File Contexts:
	 * Total file contexts: x
	 * ------------------------------------
	 * # |   Filename    | Text Offset | Data Offset | Const Offset |
	 * ---------------------------------------------------------------
	 * x |   xxxxxxx     |   0xXXXX    |	 0xXXXX    |    0xXXXX     |
	 * ---------------------------------------------------------------
	 * ...
	 * ---------------------------------------------------------------
	 */

	rtrace("====== Section Table ======");
	rtrace("Total sections: %d", sectTable->count);
	rtrace("-----------------------------------");
	rtrace(" # |  Name  |  Offset  |  Size  |");
	rtrace("-----------------------------------");
	for (uint32_t i = 0; i < sectTable->count; i++) {
		AOEFFSectHdr* sectHdr = &sectTable->sections[i];
		rtrace("%2d | %.8s | 0x%06X | 0x%06X |", i, sectHdr->shSectName, sectHdr->shSectOff, sectHdr->shSectSize);
	}
	rtrace("-----------------------------------\n");
	rtrace("---- File Contexts ----");
	rtrace("Total file contexts: %d", sectTable->filectxs.count);
	rtrace("---------------------------------------------------------------");
	rtrace(" # |   Filename    | Text Offset | Data Offset | Const Offset |");
	rtrace("---------------------------------------------------------------");
	for (uint32_t i = 0; i < sectTable->filectxs.count; i++) {
		FileCtx* filectx = &sectTable->filectxs.ctx[i];
		rtrace("%2d | %-13s |  0x%04X  |  0x%04X  |  0x%04X  |", i, filectx->filename, 
				filectx->textOffset, filectx->dataOffset, filectx->constOffset);
	}	
	rtrace("---------------------------------------------------------------");
}