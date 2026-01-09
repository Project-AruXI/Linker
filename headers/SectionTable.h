#ifndef _SECTION_TABLE_H_
#define _SECTION_TABLE_H_

#include "aoef.h"


typedef struct FileContext {
	const char* filename;
	uint32_t textOffset; // The offset where this file's text section starts in the merged text section
	uint32_t dataOffset; // The offset where this file's data section starts in the merged data section
	uint32_t constOffset; // The offset where this file's const section starts in the merged const section
	uint32_t bssOffset; // The offset where this file's bss section starts in the merged bss section
} FileCtx;


/**
 * A wrapper around the AOEFF section table structure.
 */
typedef struct SectionTable {
	AOEFFSectHdr sections[5]; // 0: .data; 1: .const; 2: .bss; 3: .text; 4: .evt;

	uint32_t sectionOffsets[5]; // Offsets of each section in the memory

	struct {
		FileCtx* ctx;
		int count;
		int cap;
	} filectxs;

	uint8_t* _data;
	uint8_t* _const;
	uint32_t* _text;
	uint8_t* _evt;
} SectionTable;

typedef enum {
	SECT_DATA,
	SECT_CONST,
	SECT_TEXT,
	SECT_EVT,
	SECT_IVT,
	SECT_BSS
} SectionType;


SectionTable* initSectionTable(uint32_t dataStart, uint32_t constStart, uint32_t bssStart, uint32_t textStart);
void deinitSectionTable(SectionTable* sectTable);

void appendSection(SectionTable* sectTable, AOEFFSectHdr* sectHdr, void* sectionData);

void addFileContext(SectionTable* sectTable, FileCtx filectx);

// void concatData(SectionTable* dest, SectionTable* src);
// void concatText(SectionTable* dest, SectionTable* src);

void displaySectionTable(SectionTable* sectTable);

#endif