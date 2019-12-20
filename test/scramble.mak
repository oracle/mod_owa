.SUFFIXES:
.SUFFIXES: .rc .h .c .o .cpp

CPP  = cl.exe
RSC  = rc.exe
LINK = link.exe
LIB  = lib.exe
RSC  = rc.exe

ORATOP = \oracle\home

APACHETOP = \Apps\Apache22

VCTOP = \Program Files\Microsoft Visual Studio

# Sometimes use /debug
LDFLAGS = /nologo /pdb:none /machine:I386 /nodefaultlib:libc \
          /subsystem:console /debug \
          /libpath:"$(VCTOP)\vc98\lib" /libpath:$(ORATOP)\lib

DEFINES= /D WIN32 /D NDEBUG /D _WINDOWS /D _MBCS /D NO_SOCKETS /D ENABLE_CRYPT /D _WIN32_WINNT=0x0501

# Sometimes use /Zi
CFLAGS = /nologo /MD /W3 /Gf /GX /c

INCS = /I "." /I "$(VCTOP)\vc98\include" /I $(ORATOP)\oci\include

WINLIBS = kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib \
          advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib \
          odbc32.lib odbccp32.lib  kernel32.lib user32.lib gdi32.lib \
          winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib \
          oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ws2_32.lib

OBJS    = owautil.obj owafile.obj owanls.obj

OCILIB = $(ORATOP)\oci\lib\msvc\oci.lib

all:  scramble.exe

scramble.exe: scramble.obj $(OBJS)
	$(LINK) $(LDFLAGS) /out:$@ scramble.obj $(OBJS) $(WINLIBS)

.c.obj:
   $(CPP) $(CFLAGS) $(DEFINES) $(INCS) $<

.cpp.obj:
   $(CPP) $(CFLAGS) $(DEFINES) $(INCS) $<

.cxx.obj:
   $(CPP) $(CFLAGS) $(DEFINES) $(INCS) $<

.rc.res:
   $(RSC) $<
