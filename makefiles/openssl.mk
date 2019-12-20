#
# Makefile for mod_owa.so (with openssl dependency)
#
# Usage:  make -kf openssl.mk
#
.SUFFIXES:
.SUFFIXES:		.lc .oc .lpc .opc .pc .c .o .cpp .oln

#
# It's assumed that you have ORACLE_HOME set in your build environment;
# if not, add a definition here to support the make process.
#
ORA_LIB         = $(ORACLE_HOME)/lib

#
# Change this to point to wherever you've installed Apache.
#
APACHE_TOP      = /usr/local/apache22

ORAINC          = -I$(ORACLE_HOME)/rdbms/demo \
		  -I$(ORACLE_HOME)/rdbms/public \
		  -I$(ORACLE_HOME)/network/public
INCLUDES        = -I. $(ORAINC) -I$(APACHE_TOP)/include

#
# CC = cc almost everywhere, but change as necessary for your platform
# (e.g. CC = gcc, or CC = /path/to/yourcc).
#
# You might need to define LD = ld on some platforms.  Mainly this matters
# in cases where the flag to build a shared library isn't understood by cc.
# Actually ld is probably correct on most platforms; it would work on
# Linux, too, except that Oracle built the OCI library in such a way that
# you will have unresolved symbols unless you use cc to link.
#
CC              = cc
LD              = cc

#
# This consists of -D<platform>, plus whatever other flags may be required
# on your platform.  In general, -O (optimization) is a good idea.  Other
# flags may be needed to improve the code, for example special flags to
# force string literals into the code segment (e.g. "-xstrconst" on Solaris).
# Some platforms require that loadable libraries be build with "position-
# independent code", and a special flag is needed here to generate such
# code (e.g. "-Z" on HP/UX).  Finally, the OCI may require certain other
# compilation flags, particularly flags that govern how structures and
# structure members are aligned and ordered, and flags that govern
# misaligned read/write operations (in general, the compiler defaults
# will be correct).
# On Linux x86_64, you may need -m64 and you definitely need -fPIC.
#
DEFS            = -D_ISOC99_SOURCE -D_GNU_SOURCE -D_LARGEFILE64_SOURCE \
		  -D_REENTRANT -DUSE_OPEN_SSL
CFLAGS          = -DAPACHE22 -DLINUX $(DEFS) -fPIC -Wall -O
#
# Mainly, LDFLAGS needs to contain whatever flag is required to cause the
# linker to generate a shared/dynamic library instead of a normal executable.
# This is different on every platform.
#
LDFLAGS         = -shared

#
# Other libraries may be needed on other platforms (e.g. "-lcl" on HP/UX).
#
CLIBS           = -L/usr/lib -ldl -lpthread -lc -lcrypto

ORALINK         = -L$(ORACLE_HOME)/lib -lclntsh

OBJS            = owautil.o owafile.o owanls.o owasql.o owalog.o \
                  owadoc.o owahand.o owaplsql.o owacache.o modowa.o

mod_owa.so: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS) $(ORALINK) $(CLIBS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<
