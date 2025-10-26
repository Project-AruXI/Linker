CC = gcc
CFLAGS = -Wall
OUT = ./out
COMP = ./components
STRUCTS = ./structures
COMMON = ./common
HEADERS = ./headers

INCLUDES = -I$(HEADERS) -I$(COMMON)

SRCS = linker.c $(COMP)/merge.c $(STRUCTS)/DyLibTable.c $(STRUCTS)/SymbolTable.c
LIBS = $(COMMON)/libargparse.a $(COMMON)/libsds.a
TARGET = $(OUT)/arxlnk

OBJS = $(SRCS:.c=.o)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

all: arxlnk

arxlnk: $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

debug: CFLAGS += -g -DDEBUG -O0
debug: arxlnk

clean:
	rm -f **/*.o
	rm linker.o