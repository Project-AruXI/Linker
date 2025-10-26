#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>

#include "diagnostics.h"

static char FN_SCOPE[64];
static char buffer[164];

static char* errnames[ERR_TOO_MANY_LIBS + 1] = {
	"INTERNAL ERROR",
	"MEMORY ERROR",
	"I/O ERROR",
	"UNDEFINED ERROR",
	"REDEFINED ERROR",
	"INVALID FORMAT",
	"TOO MANY LIBRARIES LINKED"
};

static char* warnnames[WARN_UNIMPLEMENTED + 1] = {
	"UNIMPLEMENTED FEATURE"
};

static void formatMessage(const char* fmsg, va_list args) {
	vsnprintf(buffer, sizeof(buffer), fmsg, args);
}

void emitError(errType err, const char* fmsg, ...) {
	va_list args;
	va_start(args, fmsg);

	formatMessage(fmsg, args);

	fprintf(stderr, RED "[%s]: %s%s\n", errnames[err], buffer, RESET);
	exit(-1);
}

void emitWarning(warnType warn, const char* fmsg, ...) {
	va_list args;
	va_start(args, fmsg);

	formatMessage(fmsg, args);

	fprintf(stderr, YELLOW "[%s]: %s%s\n", warnnames[warn], buffer, RESET);
}

void initScope(const char* fxnName) {
	snprintf(FN_SCOPE, sizeof(FN_SCOPE), "%s", fxnName);
}

void debug(debugLvl lvl, const char* fmsg, ...) {
#ifdef DEBUG
	va_list args;
	va_start(args, fmsg);

	formatMessage(fmsg, args);

	char* color;
	switch (lvl) {
		case DEBUG_BASIC: color = CYAN; break;
		case DEBUG_DETAIL: color = BLUE; break;
		case DEBUG_TRACE: color = MAGENTA; break;
		default: color = CYAN; break;
	}

	fprintf(stderr, "%s@%s::%s%s\n", color, FN_SCOPE, buffer, RESET);
#endif
}

void rdebug(debugLvl lvl, const char* fmsg, ...) {
#ifdef DEBUG
	va_list args;
	va_start(args, fmsg);

	formatMessage(fmsg, args);

	char* color;
	switch (lvl) {
		case DEBUG_BASIC: color = CYAN; break;
		case DEBUG_DETAIL: color = BLUE; break;
		case DEBUG_TRACE: color = MAGENTA; break;
		default: color = CYAN; break;
	}

	fprintf(stderr, "%s%s%s\n", color, buffer, RESET);
#endif
}