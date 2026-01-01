#ifndef _SECTION_TABLE_H_
#define _SECTION_TABLE_H_

#include "aoef.h"


typedef struct FileContext {
	const char* filename;
	uint32_t textOffset; // The offset where this file's text section starts in the merged text section
	uint32_t dataOffset; // The offset where this file's data section starts in the merged data section
	uint32_t constOffset; // The offset where this file's const section starts in the merged const section
} FileCtx;


/**
 * A wrapper around the AOEFF section table structure.
 */
typedef struct SectionTable {
	AOEFFSectHdr* sections;
	int count;
	int cap;

	struct {
		FileCtx* ctx;
		int count;
		int cap;
	} filectxs;

	// The total size of each section can either be stored here or computed using the section headers/file contexts
	// For example, the size of the text section is the last file context's textOffset + size of the section from section header

	uint8_t* _data;
	uint8_t* _const;
	uint32_t* _text;
} SectionTable;

typedef enum {
	SECT_DATA,
	SECT_CONST,
	SECT_TEXT,
	SECT_EVT,
	SECT_IVT,
	SECT_BSS
} SectionType;


SectionTable* initSectionTable();
void deinitSectionTable(SectionTable* sectTable);

void appendSection(SectionTable* sectTable, AOEFFSectHdr* sectHdr, void* sectionData);

void addFileContext(SectionTable* sectTable, FileCtx filectx);

void concatData(SectionTable* dest, SectionTable* src);
void concatText(SectionTable* dest, SectionTable* src);

#endif