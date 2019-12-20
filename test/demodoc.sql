Rem
Rem mod_owa
Rem
Rem Copyright (c) 1999-2019 Oracle Corporation, All rights reserved.
Rem
Rem The Universal Permissive License (UPL), Version 1.0
Rem
Rem Subject to the condition set forth below, permission is hereby granted
Rem to any person obtaining a copy of this software, associated documentation
Rem and/or data (collectively the "Software"), free of charge and under any
Rem and all copyright rights in the Software, and any and all patent rights
Rem owned or freely licensable by each licensor hereunder covering either
Rem (i) the unmodified Software as contributed to or provided by such licensor,
Rem or (ii) the Larger Works (as defined below), to deal in both
Rem 
Rem (a) the Software, and
Rem (b) any piece of software and/or hardware listed in the lrgrwrks.txt file
Rem if one is included with the Software (each a "Larger Work" to which the
Rem Software is contributed by such licensors),
Rem 
Rem without restriction, including without limitation the rights to copy,
Rem create derivative works of, display, perform, and distribute the Software
Rem and make, use, sell, offer for sale, import, export, have made, and have
Rem sold the Software and the Larger Work(s), and to sublicense the foregoing
Rem rights on either these or other terms.
Rem 
Rem This license is subject to the following condition:
Rem The above copyright notice and either this complete permission notice or
Rem at a minimum a reference to the UPL must be included in all copies or
Rem substantial portions of the Software.
Rem 
Rem THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
Rem IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
Rem FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
Rem THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
Rem LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
Rem FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
Rem DEALINGS IN THE SOFTWARE.
Rem

REM
REM Document demonstration package
REM

create or replace package DEMO_DOCUMENTS_PKG is
  --
  type VCTAB is table of varchar2(32767) index by binary_integer;
  --
  function AUTHORIZE return boolean;
  --
  procedure SHOWERROR(PROC in varchar2, ERRCODE in number);
  --
  procedure SHOW_DIRECTORY(DIR_ID in number);
  --
  procedure SET_COLORS(FID in varchar2, SCHEME in varchar2);
  --
  procedure DELETE_FILE(FID in varchar2);
  --
  procedure NEW_FOLDER(FID in varchar2, FNAME in varchar2);
  --
  procedure MAKE_LINK(FID in varchar2, FNAME in varchar2);
  --
  procedure RENAME_FOLDER(FID in varchar2, FNAME in varchar2);
  --
  procedure WRITE_VIA_FS(NUM_ENTRIES in number, NAME_ARRAY in VCTAB,
                         VALUE_ARRAY in VCTAB, RESERVED in VCTAB,
                         MTYPE in out varchar2, FNAME in out varchar2);
  --
  procedure READ_VIA_FS(NUM_ENTRIES in number, NAME_ARRAY in VCTAB,
                        VALUE_ARRAY in VCTAB, RESERVED in VCTAB,
                        MTYPE in out varchar2, FNAME in out varchar2);
  --
  procedure WRITEFILE(NUM_ENTRIES in number, NAME_ARRAY in VCTAB,
                      VALUE_ARRAY in VCTAB, RESERVED in VCTAB,
                      MTYPE in out varchar2, PBLOB out blob, PCLOB out clob);
  --
  procedure WRITEFILE(NUM_ENTRIES in number, NAME_ARRAY in VCTAB,
                      VALUE_ARRAY in VCTAB, RESERVED in VCTAB,
                      MTYPE in out varchar2, PBLOB out blob, PCLOB out clob,
                      PNLOB out nclob);
  --
  procedure WRITEFILE(NUM_ENTRIES in number, NAME_ARRAY in OWA_ARRAY,
                      VALUE_ARRAY in OWA_ARRAY, RESERVED in OWA_ARRAY,
                      MTYPE in out varchar2, PBLOB out blob, PCLOB out clob);
  --
  procedure WRITEFILE(NUM_ENTRIES in number, NAME_ARRAY in VCTAB,
                      VALUE_ARRAY in VCTAB, RESERVED in VCTAB,
                      MTYPE in out varchar2, STMT out varchar2,
                      SBIND out varchar2, RAWFLAG out varchar2);
  --
  procedure WRITEFILE(NUM_ENTRIES in number, NAME_ARRAY in VCTAB,
                      VALUE_ARRAY in VCTAB, RESERVED in VCTAB,
                      MTYPE in out varchar2, FPATH in out varchar2);
  --
  procedure READFILE(NUM_ENTRIES in number, NAME_ARRAY in VCTAB,
                     VALUE_ARRAY in VCTAB, RESERVED in VCTAB,
                     MTYPE out varchar2, PBLOB out blob, PCLOB out clob);
  --
  procedure READFILE(NUM_ENTRIES in number, NAME_ARRAY in VCTAB,
                     VALUE_ARRAY in VCTAB, RESERVED in VCTAB,
                     MTYPE out varchar2, PBLOB out blob, PCLOB out clob,
                     PNLOB out nclob);
  --
  procedure READFILE(NUM_ENTRIES in number, NAME_ARRAY in VCTAB,
                     VALUE_ARRAY in VCTAB, RESERVED in VCTAB,
                     MTYPE out varchar2, PBLOB out blob, PCLOB out clob,
                     PNLOB out nclob, PBFILE out bfile);
  --
  procedure READFILE(NUM_ENTRIES in number, NAME_ARRAY in VCTAB,
                     VALUE_ARRAY in VCTAB, RESERVED in VCTAB);
  --
  procedure READFILE(NUM_ENTRIES in number, NAME_ARRAY in OWA_ARRAY,
                     VALUE_ARRAY in OWA_ARRAY, RESERVED in OWA_ARRAY,
                     MTYPE out varchar2, PBLOB out blob, PCLOB out clob);
  --
  procedure READFILE(NUM_ENTRIES in number, NAME_ARRAY in VCTAB,
                     VALUE_ARRAY in VCTAB, RESERVED in VCTAB,
                     MTYPE out varchar2, STMT out varchar2,
                     SBIND out varchar2, RAWFLAG out varchar2);
  --
  procedure READFILE(NUM_ENTRIES in number, NAME_ARRAY in VCTAB,
                     VALUE_ARRAY in VCTAB, RESERVED in VCTAB,
                     MTYPE out varchar2, STMT out varchar2,
                     SBIND out varchar2, CLEN out number,
                     RAWFLAG out varchar2);
  --
  procedure READFILE2(NUM_ENTRIES in number, NAME_ARRAY in VCTAB,
                      VALUE_ARRAY in VCTAB, RESERVED in VCTAB,
                      MTYPE out varchar2, STMT out varchar2,
                      SBIND out varchar2, RAWFLAG out varchar2);
  --
  procedure READFILE(NUM_ENTRIES in number, NAME_ARRAY in VCTAB,
                     VALUE_ARRAY in VCTAB, RESERVED in VCTAB,
                     MTYPE out varchar2, FPATH out varchar2);
  --
  procedure UPLOAD(FILENAME in varchar2 default null);
  --
  procedure UPLOAD(NUM_ENTRIES in number, NAME_ARRAY in VCTAB,
                   VALUE_ARRAY in VCTAB, RESERVED in VCTAB);
  --
end DEMO_DOCUMENTS_PKG;
/

create or replace package body DEMO_DOCUMENTS_PKG is
  --
  type NUMTAB is table of number index by binary_integer;
  LEGACY_MODE boolean := false;
  NFILES      number := 0;
  FILE_LIST   vctab;
  DIR_LOB     clob := NULL;
  BG_COLOR    varchar2(10) := '"#ddddee"';
  HI_COLOR    varchar2(10) := '"#eeeeff"';
  LO_COLOR    varchar2(10) := '"#ccccee"';
  --
  function AUTHORIZE return boolean is
  begin
    return(TRUE);
  end AUTHORIZE;
  --
  procedure SHOWERROR(PROC in varchar2, ERRCODE in number) is
    X number;
  begin
    OWA_UTIL.MIME_HEADER('text/html', TRUE);
    HTP.P('<html><body bgcolor="#ffffff" text="#cc0000">');
    HTP.P('<p>Procedure '||PROC||' error = '||ERRCODE||'</p>');
    HTP.P('</body></html>');
  end;
  --
  function STRIP_PATH(FPATH in varchar2) return varchar2 is
    FEND  number;
    FLOC  number;
  begin
    FEND := 0;
    FLOC := 1;
    while (FLOC <> 0) loop
      FLOC := instr(FPATH, '/', FEND + 1);
      if (FLOC = 0) then
        FLOC := instr(FPATH, '\', FEND + 1);
      end if;
      if (FLOC <> 0) then
        FEND := FLOC;
      end if;
    end loop;
    if (FEND > 0) then
      return(substr(FPATH, FEND + 1));
    end if;
    return(FPATH);
  end STRIP_PATH;
  --
  function CONSTRUCT_PATH(FID in number) return varchar2 is
    SNAME varchar2(250);
    FPATH varchar2(2000);
    cursor C1 is select ID, NAME, NODE_TYPE from DEMO_DOCUMENTS
                 connect by prior PARENT_ID = ID start with ID = FID;
  begin
    FPATH := '/';
    NFILES := 0;
    for C1REC in C1 loop
      if ((C1REC.NODE_TYPE = 'D') and (C1REC.ID <> 0)) then
        NFILES := NFILES + 1;
        FILE_LIST(NFILES) := C1REC.NAME;
      end if;
    end loop;
    for I in 1..NFILES/2 loop
      SNAME := FILE_LIST(I);
      FILE_LIST(I) := FILE_LIST(NFILES - I + 1);
      FILE_LIST(NFILES - I + 1) := SNAME;
    end loop;
    for I in 1..NFILES loop
      FPATH := FPATH||FILE_LIST(I)||'/';
    end loop;
    return(FPATH);
  exception when others then
    NFILES := 0;
    return(FPATH);
  end CONSTRUCT_PATH;
  --
  function PARSE_PATH(FPATH in varchar2) return number is
    FSTART number;
    FEND   number;
    FTOTAL number;
    DIR_ID number;
    SNAME  varchar2(250);
    RNAME  varchar2(250);
  begin
    NFILES := 0;
    if (substr(FPATH,1,1) = '/') then
      FSTART := 2;
    else
      FSTART := 1;
    end if;
    FTOTAL := length(FPATH);
    DIR_ID := 0;
    while (FSTART <= FTOTAL) loop
      FEND := instr(FPATH,'/',FSTART);
      if (FEND > 0) then
        SNAME := lower(substr(FPATH, FSTART, FEND - FSTART));
        FSTART := FEND + 1;
      else
        SNAME := lower(substr(FPATH, FSTART));
        FSTART := FTOTAL + 1;
      end if;
      select ID, NAME into DIR_ID, RNAME from DEMO_DOCUMENTS
       where LNAME = SNAME and PARENT_ID = DIR_ID;
      NFILES := NFILES + 1;
      FILE_LIST(NFILES) := RNAME;
    end loop;
    return(DIR_ID);
  exception when others then
    return(DIR_ID);
  end PARSE_PATH;
  --
  procedure GET_COLORS(INSCHEME in varchar2) is
    SCHEME  varchar2(30);
    ACOOKIE owa_cookie.cookie;
  begin
    SCHEME := 'PLUM'; -- the default
    if (INSCHEME is null) then
      ACOOKIE := OWA_COOKIE.GET('DEMO_DIRECTORY_COLORS');
      if (ACOOKIE.NUM_VALS > 0) then
        SCHEME := ACOOKIE.VALS(1);
      end if;
    else
      SCHEME := INSCHEME;
    end if;
    if (SCHEME = 'GRAY') then
      BG_COLOR := '"#eeeeee"';
      HI_COLOR := '"#ffffff"';
      LO_COLOR := '"#dddddd"';
    elsif (SCHEME = 'SKY') then
      BG_COLOR := '"#bbddee"';
      HI_COLOR := '"#ddeeff"';
      LO_COLOR := '"#bbddff"';
    elsif (SCHEME = 'ROSE') then
      BG_COLOR := '"#eedddd"';
      HI_COLOR := '"#ffdddd"';
      LO_COLOR := '"#eebbbb"';
    elsif (SCHEME = 'PLUM') then
      BG_COLOR := '"#ddddee"';
      HI_COLOR := '"#eeeeff"';
      LO_COLOR := '"#ccccee"';
    elsif (SCHEME = 'LEMON') then
      BG_COLOR := '"#ffffbb"';
      HI_COLOR := '"#ffffdd"';
      LO_COLOR := '"#fff0aa"';
    elsif (SCHEME = 'LIME') then
      BG_COLOR := '"#ddeedd"';
      HI_COLOR := '"#ddffdd"';
      LO_COLOR := '"#bbeebb"';
    end if;
  end GET_COLORS;
  --
  procedure DRAW_FORM(SCRIPT in varchar2, DIR_ID in number) is
  begin
    HTP.P('<h4>Operations</h4><dir>');

    HTP.P('<table cellspacing="0" cellpadding="0" border="0">');

    HTP.P('<form action="'||SCRIPT||'demo_documents_pkg.rename_folder"');
    HTP.P(' method="GET">');
    HTP.P('<input type="hidden" name="FID" value="'||DIR_ID||'">');
    HTP.P('<tr><td><img src="/icons/ball.red.gif"></td>');
    HTP.P('<td>Rename folder </td>');
    HTP.P('<td><input type="text" size="35" name="FNAME"></td>');
    HTP.P('<td><img src="/icons/blank.gif"></td>');
    HTP.P('<td><input type="submit" value=" Rename "></td></tr></form>');

    HTP.P('<form action="'||SCRIPT||'demo_documents_pkg.new_folder"');
    HTP.P(' method="GET">');
    HTP.P('<input type="hidden" name="FID" value="'||DIR_ID||'">');
    HTP.P('<tr><td><img src="/icons/ball.red.gif"></td>');
    HTP.P('<td>New subfolder </td>');
    HTP.P('<td><input type="text" size="35" name="FNAME"></td>');
    HTP.P('<td><img src="/icons/blank.gif"></td>');
    HTP.P('<td><input type="submit" value=" Folder "></td></tr></form>');

    HTP.P('<form action="'||SCRIPT||'demo_documents_pkg.make_link"');
    HTP.P(' method="GET">');
    HTP.P('<input type="hidden" name="FID" value="'||DIR_ID||'">');
    HTP.P('<tr><td><img src="/icons/ball.red.gif"></td>');
    HTP.P('<td>Link to file </td>');
    HTP.P('<td><input type="text" size="35" name="FNAME"></td>');
    HTP.P('<td><img src="/icons/blank.gif"></td>');
    HTP.P('<td><input type="submit" value=" Link "></td></tr></form>');

    if (LEGACY_MODE) then
      HTP.P('<form action="'||SCRIPT||'~demo_documents_pkg.writefile"');
    else
      HTP.P('<form action="'||SCRIPT||'demo_documents_pkg.writefile"');
    end if;
    HTP.P(' method="POST" enctype="multipart/form-data">');
    HTP.P('<input type="hidden" name="FID" value="'||DIR_ID||'">');
    HTP.P('<tr><td><img src="/icons/ball.red.gif"></td>');
    HTP.P('<td>Upload a file </td>');
    HTP.P('<td><input type="file" size="25" name="FNAME"></td>');
    HTP.P('<td><img src="/icons/blank.gif"></td>');
    HTP.P('<td><input type="submit" value=" Upload "></td></tr></form>');

    HTP.P('<form action="'||SCRIPT||'demo_documents_pkg.set_colors"');
    HTP.P(' method="GET">');
    HTP.P('<input type="hidden" name="FID" value="'||DIR_ID||'">');
    HTP.P('<tr><td><img src="/icons/ball.red.gif"></td>');
    HTP.P('<td>Set color scheme </td>');
    HTP.P('<td><select size="1" name="SCHEME">');
    HTP.P('<option value="GRAY">Gray</option>');
    HTP.P('<option value="SKY">Sky</option>');
    HTP.P('<option value="ROSE">Rose</option>');
    HTP.P('<option value="PLUM">Plum</option>');
    HTP.P('<option value="LEMON">Lemon</option>');
    HTP.P('<option value="LIME">Lime</option>');
    HTP.P('</select></td>');
    HTP.P('<td><img src="/icons/blank.gif"></td>');
    HTP.P('<td><input type="submit" value=" Set "></td></tr></form>');

    HTP.P('</table></dir>');
  end DRAW_FORM;
  --
  procedure SHOW_DIRECTORY(DIR_ID in number) is
    IDX       number;
    LCOLOR    boolean;
    UNITS     varchar2(30);
    LPATH     varchar2(2000);
    SCRIPT    varchar2(2000);
    FULLPATH  varchar2(2000);
    XFILES    number;
    cursor C1 is select ID, NAME, NODE_TYPE, LAST_MODIFIED,
                        MIME_TYPE, LINK_ID,
                        DBMS_LOB.GETLENGTH(BIN_CONTENT) BYTES,
                        DBMS_LOB.GETLENGTH(CHAR_CONTENT) CHARS
                   from DEMO_DOCUMENTS
                  where PARENT_ID = DIR_ID order by LNAME asc;
  begin
    XFILES := NFILES;
    OWA_UTIL.MIME_HEADER('text/html', TRUE);
    GET_COLORS(null);
    SCRIPT := OWA_UTIL.GET_CGI_ENV('SCRIPT_NAME');
    FULLPATH := OWA_UTIL.GET_CGI_ENV('REQUEST_URI');
    if (SCRIPT is null) then
      SCRIPT := '/';
    end if;
    IDX := instr(FULLPATH, SCRIPT);
    if (IDX > 0) then
      IDX := IDX + length(SCRIPT) - 1;
      SCRIPT := substr(FULLPATH,1,IDX);
    end if;
    if (substr(SCRIPT,length(SCRIPT),1) <> '/') then
      SCRIPT := SCRIPT||'/';
    end if;
    FULLPATH := '';
    for I in 1..XFILES loop
      FULLPATH := FULLPATH||FILE_LIST(I)||'/';
    end loop;
    HTP.P('<head><title>'||SCRIPT||FULLPATH||'</title></head>');
    HTP.P('<html><body bgcolor='||BG_COLOR||' text="#000000" ');
    HTP.P('link="#000080" alink="#c00000" vlink="#008080">');
    HTP.P('<h4>Directory of /'||FULLPATH||'</h4>');
    HTP.P('<table cellspacing="0" cellpadding="2" border="0">');
    FULLPATH := SCRIPT;
    for I in 1..XFILES loop
      HTP.P('<tr valign="bottom"><td colspan="2">');
      for J in 1..I-1 loop
        HTP.P('<img src="/icons/blank.gif">');
      end loop;
      FULLPATH := FULLPATH||FILE_LIST(I)||'/';
      HTP.P('<a href="'||FULLPATH||'">'||
            '<img src="/icons/folder.open.gif" border="0">'||FILE_LIST(I)||
            '</a></td><td colspan="9"><img src="/icons/blank.gif"></td></tr>');
    end loop;
    LCOLOR := true;
    for C1REC in C1 loop
      if (C1REC.LINK_ID is not null) then
        select count(ID) into IDX from DEMO_DOCUMENTS
         where ID = C1REC.LINK_ID;
        if (IDX = 1) then
          LPATH := SCRIPT||substr(CONSTRUCT_PATH(C1REC.LINK_ID),2)||C1REC.NAME;
        else
          LPATH := FULLPATH;
        end if;
      elsif (C1REC.NODE_TYPE = 'D') then
        LPATH := FULLPATH||C1REC.NAME||'/';
      else
        LPATH := FULLPATH||C1REC.NAME;
      end if;
      if (lcolor) then
        HTP.P('<tr valign="bottom" nowrap bgcolor='||HI_COLOR||'>');
      else
        HTP.P('<tr valign="bottom" nowrap bgcolor='||LO_COLOR||'>');
      end if;
      LCOLOR := not LCOLOR;
      HTP.P('<td bgcolor='||BG_COLOR||'>');
      for J in 1..XFILES loop
        HTP.P('<img src="/icons/blank.gif">');
      end loop;
      HTP.P('</td><td><a href="'||LPATH||'">');
      if (C1REC.NODE_TYPE = 'D') then
        UNITS := '/icons/folder.gif';
      elsif (C1REC.NODE_TYPE = 'L') then
        if (IDX = 1) then
          UNITS := '/icons/link.gif';
        else
          UNITS := '/icons/broken.gif';
        end if;
      elsif (instr(C1REC.MIME_TYPE,'text/') = 1) then
        UNITS := '/icons/text.gif';
      elsif (instr(C1REC.MIME_TYPE,'image/') = 1) then
        UNITS := '/icons/image2.gif';
      elsif (instr(C1REC.MIME_TYPE,'audio/') = 1) then
        UNITS := '/icons/sound2.gif';
      elsif (instr(C1REC.MIME_TYPE,'video/') = 1) then
        UNITS := '/icons/movie.gif';
      elsif (instr(C1REC.MIME_TYPE,'zip') > 1) then
        UNITS := '/icons/compressed.gif';
      elsif (instr(C1REC.MIME_TYPE,'/octet-stream') > 1) then
        UNITS := '/icons/binary.gif';
      else
        UNITS := '/icons/generic.gif';
      end if;
      HTP.P('<img src="'||UNITS||'" border="0"> '||
            C1REC.NAME||'</a></td>');
      HTP.P('<td><img src="/icons/blank.gif"></td>');
      if (C1REC.NODE_TYPE = 'D') then
        HTP.P('<td><i>Folder</i></td>');
      elsif (C1REC.NODE_TYPE = 'L') then
        HTP.P('<td><i>Link</i></td>');
      else
        HTP.P('<td>'||C1REC.MIME_TYPE||'</td>');
      end if;
      HTP.P('<td><img src="/icons/blank.gif"></td>');
      if ((C1REC.NODE_TYPE = 'F') and (LEGACY_MODE)) then
        if (instr(C1REC.MIME_TYPE, 'text/') = 1) then
          UNITS := 'LONG';
        else
          UNITS := 'LONG RAW';
        end if;
      elsif (C1REC.BYTES is not null) then
        UNITS := to_char(C1REC.BYTES)||' bytes';
      elsif (C1REC.CHARS is not null) then
        UNITS := to_char(C1REC.CHARS)||' chars';
      else
        UNITS := '<img src="/icons/blank.gif">';
      end if;
      if (LEGACY_MODE) then
        HTP.P('<td align="left"><b><tt>'||UNITS||'</tt></b></td>');
      else
        HTP.P('<td align="right"><b><tt>'||UNITS||'</tt></b></td>');
      end if;
      HTP.P('<td><img src="/icons/blank.gif"></td>');
      if ((SYSDATE - C1REC.LAST_MODIFIED) > 360) then
        UNITS := to_char(C1REC.LAST_MODIFIED,'Mon DD, YYYY');
      else
        UNITS := to_char(C1REC.LAST_MODIFIED,'Mon DD HH24:MI');
      end if;
      HTP.P('<td><b><tt>'||UNITS||'</tt></b></td>');
      HTP.P('<td><img src="/icons/blank.gif"></td>');
      HTP.P('<td><a href="'||SCRIPT||'demo_documents_pkg.delete_file'||
            '?FID='||C1REC.ID||'">delete</a></td>');
      HTP.P('<td><img src="/icons/blank.gif"></td></tr>');
    end loop;
    HTP.P('</table></dir>');

    DRAW_FORM(SCRIPT, DIR_ID);

    HTP.P('</body></html>');
  end SHOW_DIRECTORY;
  --
  function CREATE_REDIRECT(FPATH in varchar2) return varchar2 is
    SCRIPT   varchar2(2000);
    FULLPATH varchar2(2000);
    IDX      number;
  begin
    SCRIPT := OWA_UTIL.GET_CGI_ENV('SCRIPT_NAME');
    FULLPATH := OWA_UTIL.GET_CGI_ENV('REQUEST_URI');
    IDX := instr(FULLPATH, SCRIPT);
    if (IDX > 0) then
      IDX := IDX + length(SCRIPT);
      SCRIPT := substr(FULLPATH,1,IDX);
    end if;
    return(SCRIPT||substr(FPATH,2));
  end CREATE_REDIRECT;
  --
  procedure SET_COLORS(FID in varchar2, SCHEME in varchar2) is
    DIR_ID  number;
    SERVER  varchar2(250);
  begin
    DIR_ID := to_number(FID);
    GET_COLORS(SCHEME);
    SERVER := OWA_UTIL.GET_CGI_ENV('SERVER_NAME');
    OWA_UTIL.MIME_HEADER('text/html', FALSE);
    OWA_COOKIE.SEND('DEMO_DIRECTORY_COLORS', SCHEME);
    OWA_UTIL.REDIRECT_URL(CREATE_REDIRECT(CONSTRUCT_PATH(DIR_ID)));
    OWA_UTIL.HTTP_HEADER_CLOSE;
  end SET_COLORS;
  --
  procedure DELETE_FILE(FID in varchar2) is
    FPATH   varchar2(2000);
    DIR_ID  number;
    FILE_ID number;
  begin
    FILE_ID := to_number(FID);
    select PARENT_ID into DIR_ID from DEMO_DOCUMENTS where ID = FILE_ID;
    FPATH := CONSTRUCT_PATH(DIR_ID);
    if (DIR_ID <> 0) then
      delete from DEMO_DOC_RAWS
       where ID in (select ID from DEMO_DOCUMENTS
                    connect by prior ID = PARENT_ID start with ID = FILE_ID);
      delete from DEMO_DOC_CHARS
       where ID in (select ID from DEMO_DOCUMENTS
                    connect by prior ID = PARENT_ID start with ID = FILE_ID);
      delete from DEMO_DOCUMENTS
       where ID in (select ID from DEMO_DOCUMENTS
                    connect by prior ID = PARENT_ID start with ID = FILE_ID);
    end if;
    OWA_UTIL.REDIRECT_URL(CREATE_REDIRECT(FPATH));
  end DELETE_FILE;
  --
  procedure NEW_FOLDER(FID in varchar2, FNAME in varchar2) is
    FPATH   varchar2(2000);
    DIR_ID  number;
    FILE_ID number;
  begin
    DIR_ID := to_number(FID);
    select DEMO_DOCUMENTS_S.NEXTVAL into FILE_ID from dual;
    insert into DEMO_DOCUMENTS (ID, PARENT_ID, NAME, LNAME, NODE_TYPE)
         values (FILE_ID, DIR_ID, FNAME, lower(FNAME), 'D');
    OWA_UTIL.REDIRECT_URL(CREATE_REDIRECT(CONSTRUCT_PATH(DIR_ID)));
  end NEW_FOLDER;
  --
  procedure MAKE_LINK(FID in varchar2, FNAME in varchar2) is
    FPATH   varchar2(2000);
    LNKNAME varchar2(250);
    DIR_ID  number;
    FILE_ID number;
    LID     number;
  begin
    DIR_ID := to_number(FID);
    LID := PARSE_PATH(FNAME);
    LNKNAME := FILE_LIST(NFILES);
    FPATH := CONSTRUCT_PATH(LID)||LNKNAME;
    if (FPATH = FNAME) then
      select DEMO_DOCUMENTS_S.NEXTVAL into FILE_ID from dual;
      insert into DEMO_DOCUMENTS (ID, PARENT_ID, NAME, LNAME,
                                  NODE_TYPE, LINK_ID)
           values (FILE_ID, DIR_ID, LNKNAME, lower(LNKNAME), 'L', LID);
    end if;
    OWA_UTIL.REDIRECT_URL(CREATE_REDIRECT(CONSTRUCT_PATH(DIR_ID)));
  end MAKE_LINK;
  --
  procedure RENAME_FOLDER(FID in varchar2, FNAME in varchar2) is
    DIR_ID  number;
    FILE_ID number;
  begin
    DIR_ID := to_number(FID);
    update DEMO_DOCUMENTS
       set NAME = FNAME, LNAME = lower(FNAME)
     where ID = DIR_ID and PARENT_ID <> 0;
    OWA_UTIL.REDIRECT_URL(CREATE_REDIRECT(CONSTRUCT_PATH(DIR_ID)));
  end RENAME_FOLDER;
  --
  procedure WRITE_VIA_FS(NUM_ENTRIES in number, NAME_ARRAY in VCTAB,
                         VALUE_ARRAY in VCTAB, RESERVED in VCTAB,
                         MTYPE in out varchar2, FNAME in out varchar2) is
  begin
    if (VALUE_ARRAY(2) is null) then
      OWA_UTIL.MIME_HEADER('text/plain', TRUE);
      HTP.P('Uploaded');
    else
      FNAME := '/'||VALUE_ARRAY(1)||'/'||VALUE_ARRAY(2);
    end if;
  end WRITE_VIA_FS;
  --
  procedure READ_VIA_FS(NUM_ENTRIES in number, NAME_ARRAY in VCTAB,
                       VALUE_ARRAY in VCTAB, RESERVED in VCTAB,
                       MTYPE in out varchar2, FNAME in out varchar2) is
    SPATH  varchar2(2000);
  begin
    SPATH := VALUE_ARRAY(1);
    FNAME := SPATH;
  end READ_VIA_FS;
  --
  procedure WRITEFILE(NUM_ENTRIES in number, NAME_ARRAY in VCTAB,
                      VALUE_ARRAY in VCTAB, RESERVED in VCTAB,
                      MTYPE in out varchar2, PBLOB out blob, PCLOB out clob) is
    DIR_ID  number;
    FILE_ID number;
    FPATH   varchar2(2000);
    TFLAG   boolean;
  begin
    PCLOB := null;
    PBLOB := null;
    DIR_ID := 0;
    if (NUM_ENTRIES > 2) then
      if (VALUE_ARRAY(3) is not null) then
        if (lower(NAME_ARRAY(3)) = 'directory') then
          FPATH := VALUE_ARRAY(3);
          DIR_ID := PARSE_PATH(FPATH);
        else
          DIR_ID := to_number(VALUE_ARRAY(3));
        end if;
      end if;
    end if;
    if (VALUE_ARRAY(2) is null) then
      if (NAME_ARRAY(2) is null) then
        OWA_UTIL.REDIRECT_URL(CREATE_REDIRECT(CONSTRUCT_PATH(DIR_ID)));
        -- FPATH := CONSTRUCT_PATH(DIR_ID);
        -- SHOW_DIRECTORY(DIR_ID);
      end if;
      return;
    end if;
    FPATH := STRIP_PATH(VALUE_ARRAY(2));
    select BIN_CONTENT, CHAR_CONTENT into PBLOB, PCLOB
      from DEMO_DOCUMENTS
     where PARENT_ID = DIR_ID and LNAME = lower(FPATH) for update;
    return;
  exception
    when NO_DATA_FOUND then begin
      select DEMO_DOCUMENTS_S.NEXTVAL into FILE_ID from dual;
      TFLAG := false;
      if (MTYPE is not null) then
        if (instr(MTYPE, 'text/') = 1) then
          TFLAG := true;
        end if;
      end if;
      if (TFLAG) then
        insert into DEMO_DOCUMENTS (ID, PARENT_ID, NAME, LNAME, NODE_TYPE,
                                    MIME_TYPE, CHAR_CONTENT)
             values (FILE_ID, DIR_ID, FPATH, lower(FPATH), 'F',
                     MTYPE, EMPTY_CLOB())
                    returning CHAR_CONTENT into PCLOB;
      else
        if (MTYPE is null) then
          MTYPE := 'application/octet-stream';
        end if;
        insert into DEMO_DOCUMENTS (ID, PARENT_ID, NAME, LNAME, NODE_TYPE,
                                    MIME_TYPE, BIN_CONTENT)
             values (FILE_ID, DIR_ID, FPATH, lower(FPATH), 'F',
                     MTYPE, EMPTY_BLOB())
                    returning BIN_CONTENT into PBLOB;
      end if;
    end;
  end WRITEFILE;
  --
  procedure WRITEFILE(NUM_ENTRIES in number, NAME_ARRAY in VCTAB,
                      VALUE_ARRAY in VCTAB, RESERVED in VCTAB,
                      MTYPE in out varchar2, PBLOB out blob, PCLOB out clob,
                      PNLOB out nclob) is
  begin
    PNLOB := null;
    WRITEFILE(NUM_ENTRIES, NAME_ARRAY, VALUE_ARRAY, RESERVED,
              MTYPE, PBLOB, PCLOB);
  end WRITEFILE;
  --
  procedure WRITEFILE(NUM_ENTRIES in number, NAME_ARRAY in OWA_ARRAY,
                      VALUE_ARRAY in OWA_ARRAY, RESERVED in OWA_ARRAY,
                      MTYPE in out varchar2, PBLOB out blob, PCLOB out clob) is
    NAME_NEW     VCTAB;
    VALUE_NEW    VCTAB;
    RESERVED_NEW VCTAB;
    PNLOB        NCLOB;
  begin
    for I in 1..NUM_ENTRIES loop
      NAME_NEW(I) := NAME_ARRAY(I);
      VALUE_NEW(I) := VALUE_ARRAY(I);
      RESERVED_NEW(I) := RESERVED(I);
    end loop;
    WRITEFILE(NUM_ENTRIES, NAME_NEW, VALUE_NEW, RESERVED_NEW,
              MTYPE, PBLOB, PCLOB, PNLOB);
  end WRITEFILE;
  --
  procedure WRITEFILE(NUM_ENTRIES in number, NAME_ARRAY in VCTAB,
                      VALUE_ARRAY in VCTAB, RESERVED in VCTAB,
                      MTYPE in out varchar2, STMT out varchar2,
                      SBIND out varchar2, RAWFLAG out varchar2) is
    FPATH   varchar2(2000);
    DIR_ID  number;
    FILE_ID number;
    TFLAG   boolean;
  begin
    RAWFLAG := null;
    STMT := null;
    SBIND := null;
    DIR_ID := 0;
    if (NUM_ENTRIES > 2) then
      if (VALUE_ARRAY(3) is not null) then
        if (lower(NAME_ARRAY(3)) = 'directory') then
          FPATH := VALUE_ARRAY(3);
          DIR_ID := PARSE_PATH(FPATH);
        else
          DIR_ID := to_number(VALUE_ARRAY(3));
        end if;
      end if;
    end if;
    if (VALUE_ARRAY(2) is null) then
      if (NAME_ARRAY(2) is null) then
        OWA_UTIL.REDIRECT_URL(CREATE_REDIRECT(CONSTRUCT_PATH(DIR_ID)));
        -- FPATH := CONSTRUCT_PATH(DIR_ID);
        -- SHOW_DIRECTORY(DIR_ID);
      end if;
      return;
    end if;
    FPATH := STRIP_PATH(VALUE_ARRAY(2));
    select ID, MIME_TYPE into FILE_ID, MTYPE from DEMO_DOCUMENTS
     where PARENT_ID = DIR_ID and LNAME = lower(FPATH);
    begin
      STMT := 'update DEMO_DOC_CHARS';
      select rowidtochar(ROWID) into SBIND
        from DEMO_DOC_CHARS where ID = FILE_ID;
      exception
        when NO_DATA_FOUND then begin
          STMT := 'update DEMO_DOC_RAWS';
          select rowidtochar(ROWID) into SBIND
            from DEMO_DOC_RAWS where ID = FILE_ID;
          RAWFLAG := 'Y';
        exception
          when NO_DATA_FOUND then
            return; /* ### ERROR ### */
        end;
    end;
    STMT := STMT||' set CONTENT = :B1 where ROWID = chartorowid(:B2)';
    return;
  exception
    when NO_DATA_FOUND then begin
      select DEMO_DOCUMENTS_S.NEXTVAL into FILE_ID from dual;
      TFLAG := false;
      if (MTYPE is null) then
        MTYPE := 'application/octet-stream';
      elsif (instr(MTYPE, 'text/') = 1) then
        TFLAG := true;
      end if;
      insert into DEMO_DOCUMENTS
                  (ID, PARENT_ID, NAME, LNAME, NODE_TYPE, MIME_TYPE)
             values (FILE_ID, DIR_ID, FPATH, lower(FPATH), 'F', MTYPE);
      if (TFLAG) then
        STMT := 'insert into DEMO_DOC_CHARS(CONTENT, ID)';
      else
        STMT := 'insert into DEMO_DOC_RAWS(CONTENT, ID)';
        RAWFLAG := 'Y';
      end if;
      STMT := STMT||' values (:B1, to_number(:B2))';
      SBIND := to_char(FILE_ID);
    end;
  end WRITEFILE;
  --
  procedure WRITEFILE(NUM_ENTRIES in number, NAME_ARRAY in VCTAB,
                      VALUE_ARRAY in VCTAB, RESERVED in VCTAB,
                      MTYPE in out varchar2, FPATH in out varchar2) is
    FNAME   varchar2(2000);
    DIR_ID  number;
    FILE_ID number;
  begin
    FPATH := null;
    DIR_ID := 0;
    if (NUM_ENTRIES > 2) then
      if (VALUE_ARRAY(3) is not null) then
        if (lower(NAME_ARRAY(3)) = 'directory') then
          DIR_ID := PARSE_PATH(VALUE_ARRAY(3));
        else
          DIR_ID := to_number(VALUE_ARRAY(3));
        end if;
      end if;
    end if;
    if (VALUE_ARRAY(2) is null) then
      if (NAME_ARRAY(2) is null) then
        OWA_UTIL.REDIRECT_URL(CREATE_REDIRECT(CONSTRUCT_PATH(DIR_ID)));
        -- FPATH := CONSTRUCT_PATH(DIR_ID);
        -- SHOW_DIRECTORY(DIR_ID);
      end if;
      return;
    end if;
    FNAME := STRIP_PATH(VALUE_ARRAY(2));
    select ID, MIME_TYPE, FILE_PATH
      into FILE_ID, MTYPE, FPATH from DEMO_DOCUMENTS
     where PARENT_ID = DIR_ID and LNAME = lower(FNAME);
    return;
  exception
    when NO_DATA_FOUND then begin
      select DEMO_DOCUMENTS_S.NEXTVAL into FILE_ID from dual;
      FPATH := CONSTRUCT_PATH(DIR_ID)||FNAME;
      insert into DEMO_DOCUMENTS
                  (ID, PARENT_ID, NAME, LNAME, NODE_TYPE, MIME_TYPE, FILE_PATH)
             values (FILE_ID, DIR_ID, FNAME, lower(FNAME), 'F', MTYPE, FPATH);
    end;
  end WRITEFILE;
  --
  procedure READFILE(NUM_ENTRIES in number, NAME_ARRAY in VCTAB,
                     VALUE_ARRAY in VCTAB, RESERVED in VCTAB,
                     MTYPE out varchar2, PBLOB out blob, PCLOB out clob) is
    FPATH     varchar2(2000);
    NPATH     varchar2(2000);
    NTYPE     char(1);
    DIR_ID    number;
  begin
    FPATH := VALUE_ARRAY(1);
    DIR_ID := PARSE_PATH(FPATH);
    if (DIR_ID > 0) then
      select NODE_TYPE, MIME_TYPE, BIN_CONTENT, CHAR_CONTENT
        into NTYPE, MTYPE, PBLOB, PCLOB
        from DEMO_DOCUMENTS where ID = DIR_ID;
      if (NTYPE = 'F') then
        return;
      end if;
    end if;
    NPATH := CONSTRUCT_PATH(DIR_ID);
    if ((NPATH = FPATH) or (NPATH = FPATH||'/')) then
      SHOW_DIRECTORY(DIR_ID);
    else
      PBLOB := null;
      PCLOB := null;
    end if;
  end READFILE;
  --
  procedure READFILE(NUM_ENTRIES in number, NAME_ARRAY in VCTAB,
                     VALUE_ARRAY in VCTAB, RESERVED in VCTAB,
                     MTYPE out varchar2, PBLOB out blob, PCLOB out clob,
                     PNLOB out nclob) is
  begin
    PNLOB := null;
    READFILE(NUM_ENTRIES, NAME_ARRAY, VALUE_ARRAY, RESERVED,
             MTYPE, PBLOB, PCLOB);
  end READFILE;
  --
  procedure READFILE(NUM_ENTRIES in number, NAME_ARRAY in VCTAB,
                     VALUE_ARRAY in VCTAB, RESERVED in VCTAB,
                     MTYPE out varchar2, PBLOB out blob, PCLOB out clob,
                     PNLOB out nclob, PBFILE out bfile) is
  begin
    PNLOB := null;
    PBFILE := null;
    READFILE(NUM_ENTRIES, NAME_ARRAY, VALUE_ARRAY, RESERVED,
             MTYPE, PBLOB, PCLOB);
  end READFILE;
  --
  procedure READFILE(NUM_ENTRIES in number, NAME_ARRAY in OWA_ARRAY,
                     VALUE_ARRAY in OWA_ARRAY, RESERVED in OWA_ARRAY,
                     MTYPE out varchar2, PBLOB out blob, PCLOB out clob) is
    NAME_NEW     VCTAB;
    VALUE_NEW    VCTAB;
    RESERVED_NEW VCTAB;
    PNLOB        NCLOB;
  begin
    for I in 1..NUM_ENTRIES loop
      NAME_NEW(I) := NAME_ARRAY(I);
      VALUE_NEW(I) := VALUE_ARRAY(I);
      RESERVED_NEW(I) := RESERVED(I);
    end loop;
    READFILE(NUM_ENTRIES, NAME_NEW, VALUE_NEW, RESERVED_NEW,
             MTYPE, PBLOB, PCLOB, PNLOB);
  end READFILE;
  --
  procedure READFILE(NUM_ENTRIES in number, NAME_ARRAY in VCTAB,
                     VALUE_ARRAY in VCTAB, RESERVED in VCTAB,
                     MTYPE out varchar2, STMT out varchar2,
                     SBIND out varchar2, RAWFLAG out varchar2) is
    NPATH  varchar2(2000);
    FPATH  varchar2(2000);
    NTYPE  char(1);
    DIR_ID number;
  begin
    RAWFLAG := null;
    STMT := null;
    SBIND := null;
    FPATH := VALUE_ARRAY(1);
    DIR_ID := PARSE_PATH(FPATH);
    if (DIR_ID > 0) then
      select NODE_TYPE, MIME_TYPE into NTYPE, MTYPE
        from DEMO_DOCUMENTS where ID = DIR_ID;
      if (NTYPE = 'F') then
        begin
          STMT := 'select CONTENT from DEMO_DOC_CHARS';
          select rowidtochar(ROWID) into SBIND
            from DEMO_DOC_CHARS where ID = DIR_ID;
          exception
            when NO_DATA_FOUND then begin
              STMT := 'select CONTENT from DEMO_DOC_RAWS';
              select rowidtochar(ROWID) into SBIND
                from DEMO_DOC_RAWS where ID = DIR_ID;
              RAWFLAG := 'Y';
          end;
        end;
        STMT := STMT||' where ROWID = chartorowid(:B1)';
        return;
      end if;
    end if;
    NPATH := CONSTRUCT_PATH(DIR_ID);
    if ((NPATH = FPATH) or (NPATH = FPATH||'/')) then
      LEGACY_MODE := true;
      SHOW_DIRECTORY(DIR_ID);
    else
      MTYPE := null;
    end if;
  end READFILE;
  --
  procedure READFILE(NUM_ENTRIES in number, NAME_ARRAY in VCTAB,
                     VALUE_ARRAY in VCTAB, RESERVED in VCTAB,
                     MTYPE out varchar2, STMT out varchar2,
                     SBIND out varchar2, CLEN out number,
                     RAWFLAG out varchar2) is
  begin
    CLEN := 0;
    READFILE(NUM_ENTRIES, NAME_ARRAY, VALUE_ARRAY, RESERVED,
             MTYPE, STMT, SBIND, RAWFLAG);
  end READFILE;
  --
  procedure READFILE(NUM_ENTRIES in number, NAME_ARRAY in VCTAB,
                     VALUE_ARRAY in VCTAB, RESERVED in VCTAB,
                     MTYPE out varchar2, FPATH out varchar2) is
    RPATH  varchar2(2000);
    NPATH  varchar2(2000);
    NTYPE  char(1);
    DIR_ID number;
  begin
    FPATH := null;
    RPATH := VALUE_ARRAY(1);
    DIR_ID := PARSE_PATH(RPATH);
    if (DIR_ID > 0) then
      select NODE_TYPE, MIME_TYPE, FILE_PATH into NTYPE, MTYPE, FPATH
        from DEMO_DOCUMENTS where ID = DIR_ID;
      if (NTYPE = 'F') then
        return;
      end if;
    end if;
    NPATH := CONSTRUCT_PATH(DIR_ID);
    if ((NPATH = RPATH) or (NPATH = RPATH||'/')) then
      LEGACY_MODE := true;
      SHOW_DIRECTORY(DIR_ID);
    else
      MTYPE := null;
    end if;
  end READFILE;
  --
  procedure READFILE2(NUM_ENTRIES in number, NAME_ARRAY in VCTAB,
                      VALUE_ARRAY in VCTAB, RESERVED in VCTAB,
                      MTYPE out varchar2, STMT out varchar2,
                      SBIND out varchar2, RAWFLAG out varchar2) is
  begin
    READFILE(NUM_ENTRIES, NAME_ARRAY, VALUE_ARRAY, RESERVED,
             MTYPE, STMT, SBIND, RAWFLAG);
    if (STMT is not null) then
      STMT := replace(STMT, 'CONTENT', 'CONTENT, 0');
    end if;
  end READFILE2;
  --
  procedure READFILE(NUM_ENTRIES in number, NAME_ARRAY in VCTAB,
                     VALUE_ARRAY in VCTAB, RESERVED in VCTAB) is
    FPATH  varchar2(2000);
    TNAME  varchar2(64);
    EXT    varchar2(64);
    STMT   varchar2(2000);
    VAL    varchar2(2000);
    LOC    number;
    ALEN   number;
    CURS   integer;
    NCOLS  integer;
    CLIST  DBMS_SQL.DESC_TAB;
    ACOLS  integer;
    ALIST  VCTAB;
    ALENS  NUMTAB;
  begin
    FPATH := VALUE_ARRAY(1);
    STMT := FPATH;
    LOC := 1;
    while (LOC > 0) loop
      LOC := instr(STMT, '/');
      if (LOC > 0) then
        STMT := substr(STMT, LOC + 1);
      end if;
    end loop;
    LOC := instr(STMT, '.');
    if (LOC > 0) then
      TNAME := substr(STMT, 1, LOC - 1);
      EXT := substr(STMT, LOC + 1);
    else
      TNAME := STMT;
      EXT := '';
    end if;
    STMT := 'select * from '||TNAME;
    CURS := DBMS_SQL.OPEN_CURSOR;
    DBMS_SQL.PARSE(CURS, STMT, DBMS_SQL.NATIVE);
    DBMS_SQL.DESCRIBE_COLUMNS(CURS, NCOLS, CLIST);
    ACOLS := 0;
    for I in 1..NCOLS loop
      LOC := CLIST(I).COL_TYPE;
      if (LOC = 1) then
        ACOLS := ACOLS + 1;
        ALIST(ACOLS) := CLIST(I).COL_NAME;
        ALENS(ACOLS) := CLIST(I).COL_MAX_LEN;
      elsif (LOC = 2) then
        ACOLS := ACOLS + 1;
        ALIST(ACOLS) := CLIST(I).COL_NAME;
        ALENS(ACOLS) := 0; -- number
      elsif (LOC = 12) then
        ACOLS := ACOLS + 1;
        ALIST(ACOLS) := CLIST(I).COL_NAME;
        ALENS(ACOLS) := -1; -- date
      end if;
    end loop;
    if (ACOLS = 0) then
      OWA_UTIL.MIME_HEADER('text/html', TRUE);
      HTP.P('<html><body bgcolor="#ddddee" text="#000000"><p>');
      HTP.P('Dynamically-generated document for ['||TNAME||']<br>');
      HTP.P('</p></body></html>');
      return;
    end if;
    STMT := 'select ';
    for I in 1..ACOLS loop
      LOC := ALENS(I);
      if (LOC = 0) then
        STMT := STMT||'to_char('||ALIST(I)||')';
      elsif (LOC < 0) then
        STMT := STMT||'to_char('||ALIST(I)||',''YYYY/MM/DD@HH24:MI:SS'')';
      else
        STMT := STMT||ALIST(I);
      end if;
      if (I < ACOLS) then
        STMT := STMT||',';
      end if;
    end loop;
    STMT := STMT||' from '||TNAME||' order by 1';
    DBMS_SQL.PARSE(CURS, STMT, DBMS_SQL.NATIVE);
    for I in 1..ACOLS loop
      LOC := ALENS(I);
      if (LOC <= 0) then
        LOC := 64;
      elsif (LOC > 2000) then
        LOC := 2000;
      end if;
      DBMS_SQL.DEFINE_COLUMN_CHAR(CURS, I, ALIST(I), LOC);
    end loop;
    LOC := DBMS_SQL.EXECUTE(CURS);
    OWA_UTIL.MIME_HEADER('text/html', TRUE);
    HTP.P('<html><body bgcolor="#ddddee" text="#000000">');
    HTP.P('<h2 align="center">'||FPATH||'</h2>');
    HTP.P('<table cellspacing="0" cellpadding="0" border="0" '||
          'bgcolor="#000000"><tr><td>');
    HTP.P('<table cellspacing="2" cellpadding="2" border="0" width="100%">');
    HTP.P('<tr valign="top" align="left" bgcolor="#ffffc0">');
    for I in 1..ACOLS loop
      HTP.P('<td><strong>'||ALIST(I)||'</strong></td>');
    end loop;
    HTP.P('</tr>');
    NCOLS := 0;
    while (DBMS_SQL.FETCH_ROWS(CURS) > 0) loop
      NCOLS := NCOLS + 1;
      HTP.P('<tr valign="top" align="left" bgcolor="#ffffff">');
      for I in 1..ACOLS loop
        DBMS_SQL.COLUMN_VALUE_CHAR(CURS, I, VAL, LOC, ALEN);
        if (ALEN = 0) then
          VAL := '<font color="#c00000"><i>null</i></font>';
        else
          VAL := substr(VAL,1,ALEN);
        end if;
        HTP.P('<td>'||VAL||'</td>');
      end loop;
      HTP.P('</tr>');
      exit when (NCOLS >= 100);
    end loop;
    DBMS_SQL.CLOSE_CURSOR(CURS);
    HTP.P('</table></td></tr></table>');
    HTP.P('</body></html>');
  exception when OTHERS then
    OWA_UTIL.MIME_HEADER('text/html', TRUE);
    HTP.P('<html><body bgcolor="#ddddee" text="#000000"><p>');
    HTP.P('Dynamically-generated document for ['||FPATH||']<br>');
    HTP.P('</p></body></html>');
  end READFILE;
  --
  procedure UPLOAD(FILENAME in varchar2 default null) is
    VBLOB  blob;
    VCTYPE varchar2(250);
  begin
    if (FILENAME is not null) then
      select BLOB_CONTENT, MIME_TYPE into VBLOB, VCTYPE
        from DEMO_DOCLOAD where NAME = FILENAME;
      OWA_UTIL.MIME_HEADER(VCTYPE, FALSE);
      HTP.P('Content-Disposition: filename="'||FILENAME||'"');
      OWA_UTIL.HTTP_HEADER_CLOSE;
      WPG_DOCLOAD.DOWNLOAD_FILE(VBLOB);
    else
      OWA_UTIL.MIME_HEADER('text/html', TRUE);
      HTP.P('<html><body bgcolor="#ffffff" text="#000000">');
      HTP.P('<form action="demo_documents_pkg.upload" method="POST"');
      HTP.P(' enctype="multipart/form-data">');
      HTP.P('<table cellspacing="3" cellpadding="3" border="0">');
      HTP.P('<tr><td align="right">File</td>');
      HTP.P('<td><input type="file" size="30" name="filename"></td></tr>');
      HTP.P('<tr><td colspan="2"><input type="submit" value=" Upload ">');
      HTP.P('</td></tr></table></form></body></html>');
    end if;
  end UPLOAD;
  --
  procedure UPLOAD(NUM_ENTRIES in number, NAME_ARRAY in VCTAB,
                   VALUE_ARRAY in VCTAB, RESERVED in VCTAB) is
  begin
    for I in 1..NUM_ENTRIES loop
      if (lower(NAME_ARRAY(I)) = 'filename') then
        UPLOAD(FILENAME => VALUE_ARRAY(I));
      end if;
    end loop;
  end UPLOAD;
  --
end DEMO_DOCUMENTS_PKG;
/
