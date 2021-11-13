#
# Copyright (c) 1999-2021 Oracle Corporation, All rights reserved.
# Licensed under the Universal Permissive License v 1.0
#       as shown at https://oss.oracle.com/licenses/upl/
#
# Makefile for mod_owa.so
#
.SUFFIXES:
.SUFFIXES:		.lc .oc .lpc .opc .pc .c .o .cpp .oln

ORA_LIB         = $(ORACLE_HOME)/lib
APACHE_TOP      = /usr/local/apache

ORAINC          = -I$(ORACLE_HOME)/rdbms/demo \
		  -I$(ORACLE_HOME)/rdbms/public \
		  -I$(ORACLE_HOME)/network/public
INCLUDES        = -I. $(ORAINC) -I$(APACHE_TOP)/include

CC              = cc
LD              = ld
CFLAGS          = -Xa -xarch=v8 -xcache=16/32/1:1024/64/1 -xchip=ultra -xstrconst -DSUN_OS5 -O
LDFLAGS         = -G

CLIBS           = -L/usr/lib -ldl -lpthread -lc

ORALINK         = -L$(ORACLE_HOME)/lib -lclntsh

OBJS            = owautil.o owafile.o owanls.o owasql.o owalog.o \
                  owadoc.o owahand.o owaplsql.o owacache.o modowa.o

mod_owa.so: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS) $(ORALINK) $(CLIBS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<
