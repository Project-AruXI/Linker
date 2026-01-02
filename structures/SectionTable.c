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