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
REM OWA alternative demstration package
REM

create or replace package DEMO_OWA is
  --
  type VC_ARR is table of varchar2(32767) index by binary_integer;
  --
  USER_ID  varchar2(255);
  PASSWORD varchar2(255);
  --
  function AUTHORIZE return boolean;
  --
  function GET_CGI_ENV(ENAME in varchar2) return varchar2;
  --
  function GET_COOKIE(CNAME in varchar2, CVALUES out VC_ARR) return number;
  --
  procedure INIT_CGI_ENV(ECOUNT in number, NAMARR in VC_ARR, VALARR in VC_ARR);
  --
  procedure SET_HEADER(HNAME in varchar2, HVALUE in varchar2);
  --
  procedure SET_COOKIE(CNAME in varchar2, CVALUE in varchar2);
  --
  procedure MIME_TYPE(MTYPE in varchar2);
  --
  procedure SET_CONTENT(PBLOB in blob);
  --
  procedure SET_CONTENT(PCLOB in clob);
  --
  procedure PRINT(BUFFER in varchar2, BLEN in number default null);
  --
  procedure PUT(BUFFER in raw, BLEN in number default null);
  --
  -- ### WARNING: THIS PROCEDURE IS JUST A HACK IMPLEMENTATION FOR TESTING
  procedure GET_PAGE(CBUFFER in out varchar2, RBUFFER out raw,
                     MOREFLAG out varchar2);
  --
  procedure GET_PAGE(HEADER in out varchar2, PBLOB out blob, PCLOB out clob);
  --
  procedure TEST_HTML;
  --
  procedure TEST_GIF;
end DEMO_OWA;
/

create or replace package body DEMO_OWA is
  CGI_COUNT    number;
  CGI_NAMES    VC_ARR;
  CGI_VALUES   VC_ARR;
  COOK_NAMES   VC_ARR;
  COOK_VALUES  VC_ARR;
  HDR_COUNT    number;
  HDR_NAMES    VC_ARR;
  HDR_VALUES   VC_ARR;
  BIN_TEMP     raw(32767);
  CHAR_TEMP    varchar2(32767);
  TEMP_LEN     number;
  BIN_CONTENT  blob;
  CHAR_CONTENT clob;
  LOB_FIRST    boolean;
  LOB_TEMP     boolean;
  LOB_BIN      boolean;
  --
  function AUTHORIZE return boolean is
  begin
    return(TRUE);
  end AUTHORIZE;
  --
  function GET_CGI_ENV(ENAME in varchar2) return varchar2 is
  begin
    for I in 1..CGI_COUNT loop
      if (CGI_NAMES(I) = upper(ENAME)) then
        return(CGI_VALUES(I));
      end if;
    end loop;
    return(null);
  end GET_CGI_ENV;
  --
  function GET_COOKIE(CNAME in varchar2, CVALUES out VC_ARR) return number is
  begin
    /* ### NOT YET IMPLEMENTED ### */
    CVALUES := COOK_VALUES;
    return(0);
  end GET_COOKIE;
  --
  procedure INIT_CGI_ENV(ECOUNT in number,
                         NAMARR in VC_ARR, VALARR in VC_ARR) is
  begin
    CGI_COUNT := 0;
    for I in 1..ECOUNT loop
      if (upper(NAMARR(I)) = 'HTTP_COOKIE') then
        null; /* ### LATER ON PARSE AND SAVE COOKIE VALUES ### */
      else
        CGI_COUNT := CGI_COUNT + 1;
        CGI_NAMES(CGI_COUNT) := upper(NAMARR(I));
        CGI_VALUES(CGI_COUNT) := VALARR(I);
      end if;
    end loop;
    HDR_NAMES(1) := 'Content-Type';
    HDR_VALUES(1) := 'text/html';
    HDR_NAMES(2) := 'Content-Length';
    HDR_VALUES(2) := null;
    HDR_COUNT := 2;
    TEMP_LEN := 0;
    BIN_TEMP := null;
    CHAR_TEMP := null;
    if (BIN_CONTENT is not null) then
      if (DBMS_LOB.ISTEMPORARY(BIN_CONTENT) <> 0) then
        DBMS_LOB.FREETEMPORARY(BIN_CONTENT);
      end if;
    end if;
    if (CHAR_CONTENT is not null) then
      if (DBMS_LOB.ISTEMPORARY(CHAR_CONTENT) <> 0) then
        DBMS_LOB.FREETEMPORARY(CHAR_CONTENT);
      end if;
    end if;
    LOB_FIRST := null;
    LOB_TEMP := null;
    LOB_BIN := null;
  end INIT_CGI_ENV;
  --
  procedure SET_HEADER(HNAME in varchar2, HVALUE in varchar2) is
  begin
    for I in 1..HDR_COUNT loop
      if (lower(HDR_NAMES(I)) = lower(HNAME)) then
        HDR_VALUES(I) := HVALUE;
        return;
      end if;
    end loop;
    HDR_COUNT := HDR_COUNT + 1;
    HDR_NAMES(HDR_COUNT) := HNAME;
    HDR_VALUES(HDR_COUNT) := HVALUE;
  end SET_HEADER;
  --
  procedure SET_COOKIE(CNAME in varchar2, CVALUE in varchar2) is
  begin
    /* ### NOT YET IMPLEMENTED ### */
    null;
  end SET_COOKIE;
  --
  procedure MIME_TYPE(MTYPE in varchar2) is
  begin
    SET_HEADER('Content-Type', MTYPE);
  end MIME_TYPE;
  --
  procedure SET_CONTENT(PBLOB in blob) is
  begin
    if ((LOB_TEMP is null) or (not LOB_TEMP)) then
      BIN_CONTENT := PBLOB;
      LOB_TEMP := false;
      LOB_BIN := true;
    end if;
  end SET_CONTENT;
  --
  procedure SET_CONTENT(PCLOB in clob) is
  begin
    if ((LOB_TEMP is null) or (not LOB_TEMP)) then
      CHAR_CONTENT := PCLOB;
      LOB_TEMP := false;
      LOB_BIN := false;
    end if;
  end SET_CONTENT;
  --
  procedure PRINT(BUFFER in varchar2, BLEN in number default null) is
    XLEN number;
  begin
    if (LOB_TEMP is null) then
      DBMS_LOB.CREATETEMPORARY(CHAR_CONTENT, FALSE);
      DBMS_LOB.OPEN(CHAR_CONTENT, DBMS_LOB.LOB_READWRITE);
      LOB_FIRST := true;
      LOB_TEMP := true;
      LOB_BIN := false;
    end if;
    if ((LOB_TEMP) and (not LOB_BIN)) then
      XLEN := lengthb(BUFFER);
      if (TEMP_LEN + XLEN < 32767) then
        CHAR_TEMP := CHAR_TEMP||BUFFER;
        TEMP_LEN := TEMP_LEN + XLEN;
      else
        if (LOB_FIRST) then
          DBMS_LOB.WRITE(CHAR_CONTENT, length(CHAR_TEMP), 1, CHAR_TEMP);
          LOB_FIRST := false;
        else
          DBMS_LOB.WRITEAPPEND(CHAR_CONTENT, length(CHAR_TEMP), CHAR_TEMP);
        end if;
        CHAR_TEMP := BUFFER;
        TEMP_LEN := XLEN;
      end if;
    end if;
  end PRINT;
  --
  procedure PUT(BUFFER in raw, BLEN in number default null) is
    XLEN number;
  begin
    if (LOB_TEMP is null) then
      DBMS_LOB.CREATETEMPORARY(BIN_CONTENT, FALSE);
      DBMS_LOB.OPEN(BIN_CONTENT, DBMS_LOB.LOB_READWRITE);
      LOB_FIRST := true;
      LOB_TEMP := true;
      LOB_BIN := true;
    end if;
    if ((LOB_TEMP) and (LOB_BIN)) then
      if (BLEN is null) then
        XLEN := UTL_RAW.LENGTH(BUFFER);
      else
        XLEN := BLEN;
      end if;
      if (TEMP_LEN + XLEN < 32767) then
        BIN_TEMP := BIN_TEMP||BUFFER;
        TEMP_LEN := TEMP_LEN + XLEN;
      else
        if (LOB_FIRST) then
          DBMS_LOB.WRITE(BIN_CONTENT, TEMP_LEN, 1, BIN_TEMP);
          LOB_FIRST := false;
        else
          DBMS_LOB.WRITEAPPEND(BIN_CONTENT, TEMP_LEN, BIN_TEMP);
        end if;
        BIN_TEMP := BUFFER;
        TEMP_LEN := XLEN;
      end if;
    end if;
  end PUT;
  --
  -- ### WARNING: THIS PROCEDURE IS JUST A HACK IMPLEMENTATION FOR TESTING
  procedure GET_PAGE(CBUFFER in out varchar2, RBUFFER out raw,
                     MOREFLAG out varchar2) is
    SEP  varchar2(30);
    XLEN number;
  begin
    MOREFLAG := null;
    SEP := CBUFFER;
    CBUFFER := '';
    RBUFFER := null;
    if (LOB_TEMP) then
      if (LOB_BIN) then
        DBMS_LOB.CLOSE(BIN_CONTENT);
      else
        DBMS_LOB.CLOSE(CHAR_CONTENT);
      end if;
      LOB_TEMP := null;
    else
      return;
    end if;
    if (LOB_BIN) then
      XLEN := UTL_RAW.LENGTH(BIN_TEMP);
    else
      XLEN := length(CHAR_TEMP);
    end if;
    SET_HEADER('Content-Length', to_char(XLEN));
    for I in 1..HDR_COUNT loop
      if (HDR_VALUES(I) is not null) then
        CBUFFER := CBUFFER||HDR_NAMES(I)||': '||HDR_VALUES(I)||SEP;
      end if;
    end loop;
    if (LOB_BIN) then
      RBUFFER := BIN_TEMP;
    else
      CBUFFER := CBUFFER||SEP||CHAR_TEMP;
    end if;
  end GET_PAGE;
  --
  procedure GET_PAGE(HEADER in out varchar2, PBLOB out blob, PCLOB out clob) is
    SEP  varchar2(30);
    XLEN number;
  begin
    SEP := HEADER;
    HEADER := '';
    PBLOB := null;
    PCLOB := null;
    if (LOB_TEMP) then
      if (LOB_BIN) then
        if (LOB_FIRST) then
          DBMS_LOB.WRITE(BIN_CONTENT, TEMP_LEN, 1, BIN_TEMP);
        else
          DBMS_LOB.WRITEAPPEND(BIN_CONTENT, TEMP_LEN, BIN_TEMP);
        end if;
        DBMS_LOB.CLOSE(BIN_CONTENT);
      else
        if (LOB_FIRST) then
          DBMS_LOB.WRITE(CHAR_CONTENT, length(CHAR_TEMP), 1, CHAR_TEMP);
        else
          DBMS_LOB.WRITEAPPEND(CHAR_CONTENT, length(CHAR_TEMP), CHAR_TEMP);
        end if;
        DBMS_LOB.CLOSE(CHAR_CONTENT);
      end if;
    end if;
    if (LOB_BIN is null) then
      XLEN := 0;
    elsif (LOB_BIN) then
      XLEN := DBMS_LOB.GETLENGTH(BIN_CONTENT);
      PBLOB := BIN_CONTENT;
    else
      XLEN := DBMS_LOB.GETLENGTH(CHAR_CONTENT);
      PCLOB := CHAR_CONTENT;
    end if;
    SET_HEADER('Content-Length', to_char(XLEN));
    for I in 1..HDR_COUNT loop
      if (HDR_VALUES(I) is not null) then
        HEADER := HEADER||HDR_NAMES(I)||': '||HDR_VALUES(I)||SEP;
      end if;
    end loop;
  end GET_PAGE;
  --
  procedure TEST_HTML is
  begin
    MIME_TYPE('text/html');
    PRINT('<html><body bgcolor=#ffffc0 text=#000000>');
    PRINT('<p>Test HTML return</p>');
    PRINT('</body></html>');
  end TEST_HTML;
  --
  procedure TEST_GIF is
    BUFFER raw(256);
  begin
    MIME_TYPE('image/gif');
    BUFFER := hextoraw('47494638396114001600C20000FFFFFF'||
                       'FF9999FF3333CCFFFF66000000000000'||
                       '000000000021FE4E5468697320617274'||
                       '20697320696E20746865207075626C69'||
                       '6320646F6D61696E2E204B6576696E20'||
                       '4875676865732C206B6576696E684065'||
                       '69742E636F6D2C2053657074656D6265'||
                       '7220313939350021F90401000003002C'||
                       '000000001400160000034038BADCFE30'||
                       'CA49AB5D25975B82171BD50181201061'||
                       'D4056479A64F21942561C34E379B2FDE'||
                       'C83C1BCAC700124E4362518842550A49'||
                       '654C73A95AAFD8AC569100003B');
    PUT(BUFFER);
  end TEST_GIF;
end DEMO_OWA;
/
