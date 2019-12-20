#
# Makefile for mod_owa.so
#
# Usage:  make -kf osx.mk
#
.SUFFIXES:
.SUFFIXES:		.lc .oc .lpc .opc .pc .c .o .cpp .oln

#
# Define location of Oracle Instant Client
#
ORACLE_HOME     = /Users/oracle/product/10.2.0

#
# It's assumed that you have ORACLE_HOME set in your build environment;
# if not, add a definition here to support the make process.
#
ORA_LIB         = $(ORACLE_HOME)/lib

#
# Change this to point to wherever you've installed Apache.
#
APACHE_TOP      = /usr/local/apache2
APR_INC         = /usr/local/apr/include/apr-1

ORAINC          = -I$(ORACLE_HOME)/rdbms/public \
		  -I$(ORACLE_HOME)/network/public \
		  -I$(ORACLE_HOME)/xdk/include

INCLUDES        = -I. $(ORAINC) -I$(APR_INC) -I$(APACHE_TOP)/include

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
CC              = gcc
LD              = gcc
RM              = /bin/rm

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
		  -D_REENTRANT
CFLAGS          = -dynamiclib -DOSX -DAPACHE24 $(DEFS) -Wall -O
#
# Mainly, LDFLAGS needs to contain whatever flag is required to cause the
# linker to generate a shared/dynamic library instead of a normal executable.
# This is different on every platform.
#
LDFLAGS         = -flat_namespace -bundle -undefined suppress

#
# Other libraries may be needed on other platforms (e.g. "-lcl" on HP/UX).
#
CLIBS           = -L/usr/local/apr/lib -L/usr/lib -ldl -lpthread -lc

ORALINK         = -L$(ORACLE_HOME)/lib -lclntsh

#
# Build the target stubs against this library, which forces older glibc
# dependencies to be built into the binary and ensures compatibility with
# older versions of Linux.
#
ORASTUBS        = $(ORACLE_HOME)/lib/stubs/libc-2.3.4-stub.so

OBJS            = owautil.o owafile.o owanls.o owasql.o owalog.o \
                  owadoc.o owahand.o owaplsql.o owacache.o modowa.o

mod_owa.so: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS) $(ORALINK) $(CLIBS)

stubs: $(OBJS)
	$(LD) $(LDFLAGS) -o mod_owa.so $(OBJS) $(ORALINK) $(ORASTUBS) $(CLIBS)

clean:
        $(RM) *.o *.so

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<
