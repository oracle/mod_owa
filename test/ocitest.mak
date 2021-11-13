#
# Copyright (c) 1999-2021 Oracle Corporation, All rights reserved.
# Licensed under the Universal Permissive License v 1.0
#       as shown at https://oss.oracle.com/licenses/upl/
#
.SUFFIXES:
.SUFFIXES: .rc .h .c .o .cpp

CPP  = cl.exe
RSC  = rc.exe
LINK = link.exe
LIB  = lib.exe
RSC  = rc.exe

ORATOP = \Home\Oracle\product\12.1.0\client_1

APACHETOP = \Home\Apache24

VCTOP = \Program Files\Microsoft SDKs\Windows\v6.0\vc
SDKTOP = \Program Files\Microsoft SDKs\Windows\v6.0

# Sometimes use /debug
# Use /machine:x86 for 32-bit
LDFLAGS = /nologo /pdb:none /machine:x64 /nodefaultlib:libc \
          /subsystem:console \
          /libpath:"$(VCTOP)\lib\x64" /libpath:"$(SDKTOP)\lib\x64" \
          /libpath:$(ORATOP)\lib

DEFINES= /D WIN64 /D NDEBUG /D _WINDOWS /D _MBCS /D WINVER=0x0501

# Use /Zi to enable debugging
CFLAGS = /nologo /MT /W3 /O2 /c

INCS = /I "." /I "$(VCTOP)\vc\include" /I $(ORATOP)\oci\include

WINLIBS = kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib \
          advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib \
          odbc32.lib odbccp32.lib  kernel32.lib user32.lib gdi32.lib \
          winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib \
          oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ws2_32.lib

OCILIB = $(ORATOP)\oci\lib\msvc\oci.lib

all:  ocitest.exe

ocitest.exe: ocitest.obj
	$(LINK) $(LDFLAGS) /out:$@ ocitest.obj $(OCILIB) $(WINLIBS)

.c.obj:
   $(CPP) $(CFLAGS) $(DEFINES) $(INCS) $<

.cpp.obj:
   $(CPP) $(CFLAGS) $(DEFINES) $(INCS) $<

.cxx.obj:
   $(CPP) $(CFLAGS) $(DEFINES) $(INCS) $<

.rc.res:
   $(RSC) $<
