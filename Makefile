CC = gcc
CFLAGS = -Wall
OUT = ./out
COMP = ./components
STRUCTS = ./structures
COMMON = ./common
HEADERS = ./headers

INCLUDES = -I$(HEADERS) -I$(COMMON)

SRCS = linker.c $(COMP)/merge.c $(COMP)/diagnostics.c $(COMP)/relocate.c $(COMP)/binwriter.c \
	$(COMP)/import.c $(COMP)/export.c $(COMP)/jumptable-reloc.c \
	$(STRUCTS)/SymbolTable.c $(STRUCTS)/SectionTable.c $(STRUCTS)/RelocTable.c
LIBS = $(COMMON)/libargparse.a $(COMMON)/libsds.a
TARGET = $(OUT)/arxlnk

ifeq ($(MAKECMDGOALS),windows)
	TARGET := $(OUT)/arxlnk.exe
endif

OBJS = $(SRCS:.c=.o)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

all: arxlnk

arxlnk: $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

windows: CC = zig cc
windows: CFLAGS += --target=x86_64-windows -g -O0
windows: LIBS = $(COMMON)/libargparse-win.a $(COMMON)/libsds-win.a
windows: arxlnk

debug: CFLAGS += -g -DDEBUG -O0
debug: arxlnk

sample: debug
	(cd ./samples/ && arx build lib && arx build exe)
	mv ./samples/out/std.adlib ../Emulator/out
	mv ./samples/out/file.arx ../Emulator/out

	(cd ../Kernel/ && arx build)
	mv ../Kernel/kern.ark ../Emulator/out

clean:
	rm -f **/*.o
	rm linker.o