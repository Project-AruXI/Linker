#include <stdlib.h>
#include <string.h>

#include "SectionTable.h"
#include "diagnostics.h"

SectionTable* initSectionTable() {
	SectionTable* sectTable = (SectionTable*) malloc(sizeof(SectionTable));
	if (!sectTable) emitError(ERR_MEM, "Failed to allocate memory for section table");

	strcpy(sectTable->sections[0].shSectName, ".data"); // .data
	sectTable->sections[0].shSectOff = 0;
	sectTable->sections[0].shSectSize = 0;

	strcpy(sectTable->sections[1].shSectName, ".const"); // .const
	sectTable->sections[1].shSectOff = 0;
	sectTable->sections[1].shSectSize = 0;

	strcpy(sectTable->sections[2].shSectName, ".bss"); // .bss
	sectTable->sections[2].shSectOff = 0;
	sectTable->sections[2].shSectSize = 0;

	strcpy(sectTable->sections[3].shSectName, ".text"); // .text
	sectTable->sections[3].shSectOff = 0;
	sectTable->sections[3].shSectSize = 0;

	sectTable->filectxs.ctx = (FileCtx*) malloc(sizeof(FileCtx) * 2);
	if (!sectTable->filectxs.ctx) emitError(ERR_MEM, "Failed to allocate memory for file contexts");
	sectTable->filectxs.count = 0;
	sectTable->filectxs.cap = 2;

	sectTable->_data = NULL;
	sectTable->_const = NULL;
	sectTable->_text = NULL;

	return sectTable;
}

void deinitSectionTable(SectionTable* sectTable) {
}

void appendSection(SectionTable* sectTable, AOEFFSectHdr* sectHdr, void*) {
	AOEFFSectHdr* targetSectHdr = NULL;

	if (sectHdr->shSectName[1] == 't') { // .text
		targetSectHdr = &sectTable->sections[3];
	} else if (sectHdr->shSectName[1] == 'd') { // .data
		targetSectHdr = &sectTable->sections[0];
	} else if (sectHdr->shSectName[1] == 'c') { // .const
		targetSectHdr = &sectTable->sections[1];
	} else if (sectHdr->shSectName[1] == 'b') { // .bss
		targetSectHdr = &sectTable->sections[2];
	} else {
		emitError(ERR_INVALID_FORMAT, "Unsupported section type: %.8s", sectHdr->shSectName);
	}

	strcpy(targetSectHdr->shSectName, sectHdr->shSectName);
	targetSectHdr->shSectOff = 0x0; // Will be updated later
	targetSectHdr->shSectSize += sectHdr->shSectSize;
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
	rtrace("====== Section Table ======");
	rtrace("-----------------------------------");
	rtrace(" # |  Name  |  Offset  |  Size  |");
	rtrace("-----------------------------------");
	for (uint32_t i = 0; i < 4; i++) {
		AOEFFSectHdr* sectHdr = &sectTable->sections[i];
		rtrace("%2d | %-6s | 0x%06X | 0x%06X |", i, sectHdr->shSectName, sectHdr->shSectOff, sectHdr->shSectSize);
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