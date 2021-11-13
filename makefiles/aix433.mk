#
# Copyright (c) 1999-2021 Oracle Corporation, All rights reserved.
# Licensed under the Universal Permissive License v 1.0
#       as shown at https://oss.oracle.com/licenses/upl/
#
# Makefile for mod_owa.so
#
.SUFFIXES:
.SUFFIXES:		.lc .oc .lpc .opc .pc .c .o .cpp .oln

MODOWA_HOME	= /oracledb/modowa-GNU-COMPILED/AP1327 
ORA_LIB         = $(ORACLE_HOME)/lib
APACHE_TOP      = /oracledb/apache_1.3.27

ORAINC          = -IBLA -I$(ORACLE_HOME)/rdbms/demo \
		  -I$(ORACLE_HOME)/rdbms/public \
		  -I$(ORACLE_HOME)/network/public

INCLUDES        = -I$(MODOWA_HOME) \
                  -I/usr/include\
                  -I$(ORAINC) \
                  -I$(APACHE_TOP)/include

CC              = gcc
#CC              = cc
LD              = ld

CFLAGS          = -DUNIX -O

LDFLAGS         =  -berok -brtl -bnortllib -bnosymbolic -bautoexp -bexpall -bM:SRE -bnoentry

#CLIBS           = -L/oracledb/freeware/lib -L/oracledb/freeware/GNUPro/lib -L/usr/lib -ldl -lpthread -lc -bI:/oracledb/apache1/libexec/httpd.exp
CLIBS           = -L/usr/local/lib -L/usr/lib -ldl -lpthread -lc -bI:/oracledb/apache1327/libexec/httpd.exp

ORALINK         = -L$(ORACLE_HOME)/lib -lclntsh

OBJS            = owautil.o owafile.o owanls.o owasql.o owalog.o \
                  owadoc.o owahand.o owaplsql.o owacache.o modowa.o

mod_owa.so: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS) $(ORALINK) $(CLIBS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<
