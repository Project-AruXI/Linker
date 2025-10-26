#ifndef _DIAGNOSTICS_H_
#define _DIAGNOSTICS_H_

#define RESET "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"
#define WHITE "\033[37m"

typedef enum {
	ERR_INTERNAL,
	ERR_MEM,
	ERR_IO,
	ERR_UNDEFINED,
	ERR_REDEFINED,
	ERR_INVALID_FORMAT,
	ERR_TOO_MANY_LIBS
} errType;

typedef enum {
	WARN_UNIMPLEMENTED,
} warnType;

void emitError(errType err, const char* fmsg, ...);
void emitWarning(warnType warn, const char* fmsg, ...);

typedef enum {
	DEBUG_BASIC,
	DEBUG_DETAIL,
	DEBUG_TRACE
} debugLvl;

void initScope(const char* fxnName);

void debug(debugLvl lvl, const char* fmsg, ...);
void rdebug(debugLvl lvl, const char* fmsg, ...);

#define log(fmt, ...) debug(DEBUG_BASIC, fmt, ##__VA_ARGS__)
#define detail(fmt, ...) debug(DEBUG_DETAIL, fmt, ##__VA_ARGS__)
#define trace(fmt, ...) debug(DEBUG_TRACE, fmt, ##__VA_ARGS__)

#define rlog(fmt, ...) rdebug(DEBUG_BASIC, fmt, ##__VA_ARGS__)
#define rdetail(fmt, ...) rdebug(DEBUG_DETAIL, fmt, ##__VA_ARGS__)
#define rtrace(fmt, ...) rdebug(DEBUG_TRACE, fmt, ##__VA_ARGS__)

#endif