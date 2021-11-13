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

LDFLAGS = /nologo /dll /pdb:none /machine:I386 /nodefaultlib:libc \
          /libpath:"\devstudio\vc98\lib" /libpath:"\oracle\lib"
DEFINES= /D WIN32 /D NDEBUG /D _WINDOWS /D _MBCS /D _USRDLL /D WITH_OCI
CFLAGS = /nologo /MD /W3 /GX /O2 /c
INCS = /I "." /I "\apps\apache\src\include" \
       /I "\apps\apache\src\os\win32" \
       /I "\devstudio\vc98\include" /I "\oracle\include\oci"

WINLIBS = kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib \
          advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib \
          odbc32.lib odbccp32.lib  kernel32.lib user32.lib gdi32.lib \
          winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib \
          oleaut32.lib uuid.lib odbc32.lib odbccp32.lib

OBJS   = modtest.obj

all:  mod_test.dll

mod_test.dll: $(OBJS)
	$(LINK) $(LDFLAGS) /out:$@ $(OBJS) \
	\apps\apache\lib\ApacheCore.lib \oracle\lib\oci.lib $(WINLIBS)

.c.obj:
   $(CPP) $(CFLAGS) $(DEFINES) $(INCS) $<

.cpp.obj:
   $(CPP) $(CFLAGS) $(DEFINES) $(INCS) $<

.cxx.obj:
   $(CPP) $(CFLAGS) $(DEFINES) $(INCS) $<

.rc.res:
   $(RSC) $<
