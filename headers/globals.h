#ifndef _GLOBALS_H
#define _GLOBALS_H

#include "SectionTable.h"
#include "SymbolTable.h"
#include "RelocTable.h"

typedef struct GlobalTables {
	SectionTable* sectionTable;
	SymbolTable* symbolTable;
	RelocTable* relocTable;
} GlobalTables;

#endif