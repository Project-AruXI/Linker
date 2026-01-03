#ifndef _RELOCATE_H_
#define _RELOCATE_H_

#include "RelocTable.h"
#include "SectionTable.h"
#include "SymbolTable.h"

void relocate(RelocTable* relocTable, SectionTable* sectTable, SymbolTable* symbTable);

#endif