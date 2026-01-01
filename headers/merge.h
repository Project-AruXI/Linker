#ifndef _MERGE_H_
#define _MERGE_H_

#include "SectionTable.h"
#include "SymbolTable.h"
#include "RelocTable.h"

typedef struct GlobalTables {
	SectionTable* sectionTable;
	SymbolTable* symbolTable;
	RelocTable* relocTable;
	char* stringTable;
} GlobalTables;

void merge(const char* infile, GlobalTables* globalTables);

#endif