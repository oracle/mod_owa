#
# Makefile for mod_test.so
#
.SUFFIXES:
.SUFFIXES:		.lc .oc .lpc .opc .pc .c .o .cpp .oln

ORA_LIB         = $(ORACLE_HOME)/lib
APACHE_TOP      = /usr/local/apache24

ORAINC          = -I$(ORACLE_HOME)/rdbms/demo \
		  -I$(ORACLE_HOME)/rdbms/public \
		  -I$(ORACLE_HOME)/network/public \
		  -I$(ORACLE_HOME)/xdk/include
INCLUDES        = -I. $(ORAINC) -I$(APACHE_TOP)/include

CC              = cc
LD              = cc

DEFS            = -D_ISOC99_SOURCE -D_GNU_SOURCE -D_LARGEFILE64_SOURCE \
		  -D_REENTRANT -DUSE_OPEN_SSL
CFLAGS          = -DAPACHE24 -DLINUX $(DEFS) -fPIC -Wall -g

LDFLAGS         = -shared

CLIBS           = -L/usr/lib -ldl -lpthread -lc -lcrypto

ORALINK         = -L$(ORACLE_HOME)/lib -lclntsh

OBJS            = modtest.o

mod_test.so: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS) $(ORALINK) $(CLIBS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<
