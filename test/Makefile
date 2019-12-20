#
# Makefile for mod_owa.so
#
.SUFFIXES:
.SUFFIXES:		.lc .oc .lpc .opc .pc .c .o .cpp .oln

ORA_LIB         = $(ORACLE_HOME)/lib

ORAINC          = -I$(ORACLE_HOME)/rdbms/demo \
		  -I$(ORACLE_HOME)/rdbms/public \
		  -I$(ORACLE_HOME)/network/public \
		  -I$(ORACLE_HOME)/xdk/include

INCLUDES        = -I. $(ORAINC)

CC              = cc
LD              = cc
DEFS            = -D_ISOC99_SOURCE -D_GNU_SOURCE -D_LARGEFILE64_SOURCE \
		  -D_REENTRANT -DUSE_OPEN_SSL
CFLAGS          = -DLINUX $(DEFS) -Wall -fPIC -g
LDFLAGS         = -dynamic

CLIBS           = -L/usr/lib -ldl -lpthread -lc -lcrypto

ORALINK         = -L$(ORACLE_HOME)/lib -lclntsh

OBJS            = owautil.o owafile.o owanls.o

all: ocitest scramble

ocitest: ocitest.o
	$(LD) -o $@ ocitest.o $(ORALINK) $(CLIBS)

scramble: scramble.o $(OBJS)
	$(LD) -o $@ scramble.o $(OBJS) $(ORALINK) $(CLIBS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<
