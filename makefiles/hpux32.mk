#
# Makefile for mod_owa.sl
#
.SUFFIXES:
.SUFFIXES:		.lc .oc .lpc .opc .pc .c .o .cpp .oln

ORA_LIB         = $(ORACLE_HOME)/lib
APACHE_TOP      = /u01/app/apache

ORAINC          = -I$(ORACLE_HOME)/rdbms/demo \
		  -I$(ORACLE_HOME)/rdbms/public \
		  -I$(ORACLE_HOME)/network/public
INCLUDES        = -I. -I$(APACHE_TOP)/include $(ORAINC)

CC              = cc
LD              = ld
DEFS            = -D_HPUX_SOURCE -DHP9000S800 -DNLS_ASIA -DHPUX_KTHREAD
CFLAGS          = -DHPUX11 -DUNIX +ESlit +ESsfc +DAportable +DS2.0 +Z -O
LDFLAGS         = -G -B symbolic -b +s

CLIBS           = -L/usr/lib -lpthread -lcl -lc

ORALINK         = -L$(ORA_LIB) -lclntsh

OBJS            = owautil.o owafile.o owanls.o owasql.o owalog.o \
                  owadoc.o owahand.o owaplsql.o owacache.o modowa.o

mod_owa.sl: $(OBJS)
	$(LD) $(LDFLAGS) +e owa_module -o $@ $(OBJS) $(ORALINK) $(CLIBS)

.c.o:
	$(CC) $(DEFS) $(CFLAGS) $(INCLUDES) -c $<
