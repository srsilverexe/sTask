TARGET=sTask
CC=gcc
DEBUG=-g
OPT=-O0
WARN=-Wall
CURSES=-lncurses
TINFO=-ltinfo
PANEL=-lpanel
FORM=-lform
CCFLAGS=$(DEBUG) $(OPT) $(WARN)
LD=gcc
OBJS=src/main.c

all: $(OBJS)
	$(LD) -o $(TARGET) $(OBJS) $(DEBUG) $(OPT) $(WARN) $(CURSES) $(TINFO) $(PANEL) $(FORM)
