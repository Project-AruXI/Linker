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
DyLibTable* dynLibBuild(SymbolTable* globalSymTable, const char** libPath, char* libs[32]);