#
# Makefile for mod_owa.so
#
.SUFFIXES:
.SUFFIXES:		.lc .oc .lpc .opc .pc .c .o .cpp .oln

ORA_LIB         = $(ORACLE_HOME)/lib
APACHE_TOP      = /usr/local/apache24

ORAINC          = -I$(ORACLE_HOME)/rdbms/demo \
		  -I$(ORACLE_HOME)/rdbms/public \
		  -I$(ORACLE_HOME)/network/public
INCLUDES        = -I. $(ORAINC) -I$(APACHE_TOP)/include

CC              = cc
LD              = cc
DEFS            = -D_ISOC99_SOURCE -D_GNU_SOURCE -D_LARGEFILE64_SOURCE \
		  -D_REENTRANT
CFLAGS          = -DAPACHE24 -DLINUX $(DEFS) -fPIC -Wall -O
LDFLAGS         = -shared

CLIBS           = -L/usr/lib -ldl -lpthread -lc

# Needed for Solaris link:
NLIBS           = -L/usr/lib -lsocket -lnsl

ORALINK         = -L$(ORACLE_HOME)/lib -lclntsh

OBJS            = owautil.o owafile.o owanls.o owasql.o \
                  owadoc.o owahand.o owaplsql.o owacache.o

owad: owad.o $(OBJS)
	$(LD) -o $@ owad.o $(OBJS) $(ORALINK) $(CLIBS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<
