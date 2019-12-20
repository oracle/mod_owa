.SUFFIXES:
.SUFFIXES: .rc .h .c .o .cpp

CPP  = cl.exe
RSC  = rc.exe
LINK = link.exe
LIB  = lib.exe
RSC  = rc.exe

ORATOP = \Home\Win32\OracleXE\app\oracle\product\11.2.0\server

APACHETOP = \Home\Win32\Apache24

VCTOP = \Program Files\Microsoft SDKs\Windows\v6.0\vc
SDKTOP = \Program Files\Microsoft SDKs\Windows\v6.0

# Sometimes use /debug
# Use /machine:x64 for 64-bit
LDFLAGS = /nologo /dll /pdb:none /machine:x86 /nodefaultlib:libc \
          /libpath:"$(VCTOP)\lib" /libpath:"$(SDKTOP)\lib" \
          /libpath:$(ORATOP)\lib

# Change APACHE24 to APACHE22 or APACHE20 or EAPI
DEFINES= /D WIN32 /D NDEBUG /D _WINDOWS /D _MBCS /D _USRDLL /D APACHE24

# Use /Zi to enable debugging
CFLAGS = /nologo /MT /W3 /O2 /c

INCS = /I "." \
       /I $(APACHETOP)\include \
       /I $(APACHETOP)\src\include /I $(APACHETOP)\src\os\win32 \
       /I "$(VCTOP)\vc\include" \
       /I $(ORATOP)\oci\include

WINLIBS = kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib \
          advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib \
          odbc32.lib odbccp32.lib  kernel32.lib user32.lib gdi32.lib \
          winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib \
          oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ws2_32.lib

OBJS    = owautil.obj owafile.obj owanls.obj owasql.obj owalog.obj \
          owahand.obj owaplsql.obj owadoc.obj owacache.obj modowa.obj


OCILIB = $(ORATOP)\oci\lib\msvc\oci.lib

APACHE13LIBS = $(APACHETOP)\libexec\ApacheCore.lib

APACHE20LIBS = $(APACHETOP)\lib\libhttpd.lib \
               $(APACHETOP)\lib\libapr.lib \
               $(APACHETOP)\lib\libaprutil.lib

APACHE22LIBS = $(APACHETOP)\lib\libhttpd.lib \
               $(APACHETOP)\lib\libapr-1.lib \
               $(APACHETOP)\lib\libaprutil-1.lib

APACHE24LIBS = $(APACHETOP)\lib\libhttpd.lib \
               $(APACHETOP)\lib\libapr-1.lib \
               $(APACHETOP)\lib\libaprutil-1.lib

all:  mod_owa.dll

mod_owa.dll: $(OBJS)
	$(LINK) $(LDFLAGS) /out:$@ $(OBJS) \
	$(APACHE24LIBS) $(OCILIB) $(WINLIBS)

.c.obj:
   $(CPP) $(CFLAGS) $(DEFINES) $(INCS) $<

.cpp.obj:
   $(CPP) $(CFLAGS) $(DEFINES) $(INCS) $<

.cxx.obj:
   $(CPP) $(CFLAGS) $(DEFINES) $(INCS) $<

.rc.res:
   $(RSC) $<
