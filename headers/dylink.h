#include "SymbolTable.h"
#include "DyLibTable.h"
#include "aoef.h"


/**
 * Builds the dynamic library dependencies
 * @param globalSymTable 
 * @param libPath 
 * @param libs 
 * @return 
 */
DyLibTable* dynLibBuild(SymbolTable* globalSymTable, char* libPath[8], char* libs[32]);