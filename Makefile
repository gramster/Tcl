# Choose whether you want local heap, debugging, neither or both.
# If you change either of these be sure to delete all object files
# before the next make.

#DEBUG=-DGW_DEBUG
DEBUG=
#HEAP=-DLOCAL_HEAP -DCHK_USE
#HEAP=-DLOCAL_HEAP
HEAP=
MODEL=-ml
OPT=

###########################################################
# Don't change below here, except to switch between DOS/UNIX
# or to change the log file setting.

DOSOSUF=.obj
UNIXOSUF=.o
DOSXSUF=.exe
UNIXXSUF=
DOSCSUF=cpp
UNIXCSUF=cc
DOSCFLAGS=-v -ls $(DEBUG) $(MODEL)
UNIXCFLAGS=-g $(DEBUG) 
DOSLOG="gwtest.log"
UNIXLOG="\"gwtest.log\""

# UNIX OPTIONS

CCPP=g++
CC=gcc
CFLAGS=$(UNIXCFLAGS)
FLAGS=$(UNIXCFLAGS)
LFLAGS=$(UNIXCFLAGS) -o tcl
LOG=$(UNIXLOG)
OSUF=$(UNIXOSUF)
XSUF=$(UNIXXSUF)
CSUF=$(UNIXCSUF)
ZIP=zip
XTRA=-lncurses

# DOS OPTIONS

#CCPP=bcc
#CC=bcc
#FLAGS=$(DOSCFLAGS)
#CFLAGS=$(DOSCFLAGS) -H
#LFLAGS=$(DOSCFLAGS)
#LOG=$(DOSLOG)
#OSUF=$(DOSOSUF)
#XSUF=$(DOSXSUF)
#CSUF=$(DOSCSUF)
#ZIP=pkzip
#XTRA=hrtime.obj

all: tcl$(XSUF)

#all: test$(XSUF)

OBJS=tcl$(OSUF) parse$(OSUF) gstring$(OSUF) vars$(OSUF) lists$(OSUF) $(XTRA) pattern$(OSUF) file$(OSUF) awklib$(OSUF) $(XTRA)
#OBJS=tcl$(OSUF) parse$(OSUF) gstring$(OSUF) vars$(OSUF) lists$(OSUF) $(XTRA) pattern$(OSUF) file$(OSUF) $(XTRA)

tcl$(XSUF): $(OBJS)
	$(CCPP) $(LFLAGS) $(OBJS) 

file$(OSUF): file.$(CSUF) tcl.h
	$(CCPP) -c $(CFLAGS) file.$(CSUF)

tcl$(OSUF): tcl.$(CSUF) tcl.h
	$(CCPP) -c $(CFLAGS) tcl.$(CSUF)

lists$(OSUF): lists.$(CSUF) tcl.h
	$(CCPP) -c $(CFLAGS) lists.$(CSUF)

vars$(OSUF): vars.$(CSUF) tcl.h
	$(CCPP) -c $(CFLAGS) vars.$(CSUF)

parse$(OSUF): parse.$(CSUF) tcl.h
	$(CCPP) -c $(CFLAGS) parse.$(CSUF)

pattern$(OSUF): pattern.$(CSUF) tcl.h awklib.c
	$(CCPP) -c $(CFLAGS) pattern.$(CSUF)

gstring$(OSUF): gstring.$(CSUF) tcl.h
	$(CCPP) -c $(CFLAGS) gstring.$(CSUF)

tcl.h: lists.h vars.h tcltop.h gstring.h awklib.h
	touch tcl.h

zip:
	$(ZIP) -ur mytcl *.$(CSUF) *.h awklib.c makefile *.tcl todo gc4.ini

hrtime.obj: hrtime.asm
	bcc -ml -Tml -c hrtime.asm

awklib$(OSUF): awklib.c awklib.h
	$(CC) $(FLAGS) -c awklib.c

dos2unix:
	mv gstring.cpp gstring.cc
	mv parse.cpp parse.cc
	mv tcl.cpp tcl.cc
	mv vars.cpp vars.cc
	mv lists.cpp lists.cc
	mv pattern.cpp pattern.cc
	mv file.cpp file.cc
