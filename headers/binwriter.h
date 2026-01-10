#ifndef _BINWRITER_H_
#define _BINWRITER_H_

#include "globals.h"
#include "config.h"
#include "dylib.h"


void writeBinary(Config* config, GlobalTables* globalTables, struct ImportsExports* importsExports, JumpTables* jumpTables, DynamicLibraries* dyLibTable);

#endif