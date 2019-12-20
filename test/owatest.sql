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
REM Package of test procedures for mod_owa
REM

create or replace package MODOWA_TEST_PKG is
  --
  type VCTAB is table of varchar2(32767) index by binary_integer;
  --
  function AUTHORIZE return boolean;
  --
  function AUTHORIZE(PROC_NAME varchar2) return boolean;
  --
  procedure CHECK_CACHE(URI out varchar2, CHKSUM out varchar2,
                        MTYPE out varchar2, LIFE out number);
  --
  procedure TEST_ARGUMENTS(N in number default 300, A in varchar2);
  --
  procedure TEST_ARRAY(N in number, A in VCTAB);
  --
  procedure TEST_ARRAY(N in number, A in OWA_ARRAY);
  --
  procedure TEST_LARGE(NUM_ENTRIES in number, NAME_ARRAY in VCTAB,
                       VALUE_ARRAY in VCTAB, RESERVED in VCTAB);
  --
  procedure TEST_REWRITE(NAME_ARRAY in VCTAB, VALUE_ARRAY in VCTAB);
  --
  procedure TEST_TWOARG(NAME_ARRAY in VCTAB, VALUE_ARRAY in VCTAB);
  --
  procedure TEST_PROMOTION(X varchar2, Y in VCTAB);
  --
  procedure TEST_FLEX(NUM_ENTRIES in number, NAME_ARRAY in VCTAB,
                      VALUE_ARRAY in VCTAB, RESERVED in VCTAB);
  --
  procedure TEST_FLEX(NUM_ENTRIES in number, NAME_ARRAY in OWA_ARRAY,
                      VALUE_ARRAY in OWA_ARRAY, RESERVED in OWA_ARRAY);
  --
  procedure TEST_COOKIE(A in varchar2);
  --
  procedure TEST_AUTH;
  --
  procedure TEST_BLANK;
  --
  procedure TEST_STATUS(ERRCODE in varchar2 default null);
  --
  procedure TEST_RAWARG(R in raw);
  --
  procedure TEST_RAWBUF(R in raw);
  --
  procedure TEST_PLAIN;
  --
  procedure TEST_IMAGE;
  --
  procedure TEST_XML;
  --
  procedure TEST_DOCLOAD;
  --
  procedure TEST_REST(NUM_ENTRIES in number, NAME_ARRAY in VCTAB,
                      VALUE_ARRAY in VCTAB, RESERVED in VCTAB);
  --
  procedure TEST_TILDE(R in raw, MTYPE in out varchar2, PBLOB out blob);
  --
  procedure TEST_NCHAR(A in nvarchar2);
  --
  procedure TEST_REDIRECT(U in varchar2);
  --
  procedure TEST_NOHEADER(U in varchar2);
  --
  procedure TEST_SCRIPTNAME;
  --
  procedure EUROCHARS(N in number default 10);
  --
  procedure CHINESE(N in number default 10);
  --
  procedure READFILE(NUM_ENTRIES in number, NAME_ARRAY in VCTAB,
                     VALUE_ARRAY in VCTAB, RESERVED in VCTAB,
                     MTYPE out varchar2, PBLOB out blob);
  --
  procedure TEST_CURSOR(NUM_ENTRIES in number, NAME_ARRAY in VCTAB,
                        VALUE_ARRAY in VCTAB, RESERVED in VCTAB,
                        RESULT_CURSOR$ out SYS_REFCURSOR,
                        XML_TAGS$ out varchar2);
  --
end MODOWA_TEST_PKG;
/

create or replace package body MODOWA_TEST_PKG is
  --
  RETRY_FLAG  boolean := false;
  CACHE_URI   varchar2(2000) := null;
  CACHE_SUM   varchar2(2000) := null;
  --
  IMG_DATA    raw(256) := hextoraw('47494638396114001600C20000FFFFFF'||
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
  --
  function AUTHORIZE return boolean is
  begin
    return(TRUE);
  end AUTHORIZE;
  --
  function AUTHORIZE(PROC_NAME varchar2) return boolean is
    V_PREFIX varchar2(30) := 'MODOWA_TEST_PKG.';
  begin
    if (instr(V_PREFIX, upper(PROC_NAME)) <> 1) then
      return(FALSE);
    end if;
    if (instr('.', substr(upper(PROC_NAME),length(V_PREFIX)+1)) <> 0) then
      return(FALSE);
    end if;
    return(TRUE);
  end AUTHORIZE;
  --
  procedure CHECK_CACHE(URI out varchar2, CHKSUM out varchar2,
                        MTYPE out varchar2, LIFE out number) is
  begin
    URI := CACHE_URI;
    CHKSUM := CACHE_SUM;
    LIFE := 600; -- 10 minutes
    MTYPE := 'text/html';
    CACHE_URI := '';
    CACHE_SUM := '';
  end CHECK_CACHE;
  --
  function ESCAPE_URL_ARG(C in varchar2) return varchar2 is
    BUFFER varchar2(2000);
    BCHAR  varchar2(10);
    SLEN   number;
    ALPHA  varchar2(64) :=
           '0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_-';
  begin
    if (C is null) then
      SLEN := 0;
    else
      SLEN := length(C);
    end if;
    BUFFER := '';
    for I in 1..SLEN loop
      BCHAR := substr(C, I, 1);
      if (BCHAR = ' ') then
        BCHAR := '+';
      elsif (instr(ALPHA, BCHAR) = 0) then
        BCHAR := '%'||RAWTOHEX(UTL_RAW.CAST_TO_RAW(BCHAR));
      end if;
      BUFFER := BUFFER||BCHAR;
    end loop;
    return(BUFFER);
  end ESCAPE_URL_ARG;
  --
  procedure TEST_ARGUMENTS(N in number default 300, A in varchar2) is
    CACHE_FLAG varchar2(30);
  begin
    CACHE_FLAG := OWA_UTIL.GET_CGI_ENV('MODOWA_PAGE_CACHE');
    if (lower(CACHE_FLAG) = 'enabled') then
      CACHE_URI := '/docs/owapages/testargs_'||N||'_.htm';
      CACHE_SUM := A;
    else
      RETRY_FLAG := true;
    end if;
    if (RETRY_FLAG) then
      OWA_UTIL.MIME_HEADER('text/html', TRUE);
      HTP.P('<html><body bgcolor="#ddddee" text="#000000"><p>');
      for I in 1..N loop
        HTP.P(to_char(I,'9999')||': '||A||'<br>');
      end loop;
      HTP.P('</p></body></html>');
      RETRY_FLAG := false;
    else
      RETRY_FLAG := true;
    end if;
  end TEST_ARGUMENTS;
  --
  procedure TEST_ARRAY(N in number, A in VCTAB) is
  begin
    OWA_UTIL.MIME_HEADER('text/html', TRUE);
    HTP.P('<html><body bgcolor="#ddddee" text="#000000"><p>');
    for I in 1..N loop
      HTP.P(to_char(I)||': '||A(I)||'<br>');
    end loop;
    HTP.P('</p></body></html>');
  end TEST_ARRAY;
  --
  procedure TEST_ARRAY(N in number, A in OWA_ARRAY) is
    A_NEW VCTAB;
  begin
    for I in 1..N loop
      A_NEW(I) := A(I);
    end loop;
    TEST_ARRAY(N, A_NEW);
  end TEST_ARRAY;
  --
  procedure TEST_LARGE(NUM_ENTRIES in number, NAME_ARRAY in VCTAB,
                       VALUE_ARRAY in VCTAB, RESERVED in VCTAB) is
  begin
    OWA_UTIL.MIME_HEADER('text/html', TRUE);
    HTP.P('<html><body bgcolor="#ddddee" text="#000000"><pre>');
    for I in 1..NUM_ENTRIES loop
      HTP.P(NAME_ARRAY(I)||' ['||lengthb(VALUE_ARRAY(I))||']');
      HTP.P(VALUE_ARRAY(I));
    end loop;
    HTP.P('</pre></body></html>');
  end;
  --
  procedure TEST_REWRITE(NAME_ARRAY in VCTAB, VALUE_ARRAY in VCTAB) is
  begin
    TEST_LARGE(least(NAME_ARRAY.count(),VALUE_ARRAY.count()),
               NAME_ARRAY, VALUE_ARRAY, VALUE_ARRAY);
  end;
  --
  procedure TEST_PROMOTION(X varchar2, Y in VCTAB) is
  begin
    OWA_UTIL.MIME_HEADER('text/html', TRUE);
    HTP.P('<html><body bgcolor="#ddddee" text="#000000"><p>');
    for I in 1..Y.count() loop
      HTP.P(X||' = '||Y(I)||'<br>');
    end loop;
    HTP.P('</p></body></html>');
  end;
  --
  procedure TEST_TWOARG(NAME_ARRAY in VCTAB, VALUE_ARRAY in VCTAB) is
  begin
    OWA_UTIL.MIME_HEADER('text/html', TRUE);
    HTP.P('<html><body bgcolor="#ddddee" text="#000000"><p>');
    for I in 1..NAME_ARRAY.count() loop
      HTP.P(NAME_ARRAY(I)||'='||ESCAPE_URL_ARG(VALUE_ARRAY(I))||'<br>');
    end loop;
    HTP.P('</p></body></html>');
  end;
  --
  procedure TEST_FLEX(NUM_ENTRIES in number, NAME_ARRAY in VCTAB,
                      VALUE_ARRAY in VCTAB, RESERVED in VCTAB) is
  begin
    OWA_UTIL.MIME_HEADER('text/html', TRUE);
    HTP.P('<html><body bgcolor="#ddddee" text="#000000"><p>');
    for I in 1..NUM_ENTRIES loop
      HTP.P(NAME_ARRAY(I)||'='||ESCAPE_URL_ARG(VALUE_ARRAY(I))||'<br>');
    end loop;
    HTP.P('</p></body></html>');
  end;
  --
  procedure TEST_FLEX(NUM_ENTRIES in number, NAME_ARRAY in OWA_ARRAY,
                      VALUE_ARRAY in OWA_ARRAY, RESERVED in OWA_ARRAY) is
    NAME_NEW     VCTAB;
    VALUE_NEW    VCTAB;
    RESERVED_NEW VCTAB;
  begin
    for I in 1..NUM_ENTRIES loop
      NAME_NEW(I) := NAME_ARRAY(I);
      VALUE_NEW(I) := VALUE_ARRAY(I);
      RESERVED_NEW(I) := RESERVED(I);
    end loop;
    TEST_FLEX(NUM_ENTRIES, NAME_NEW, VALUE_NEW, RESERVED_NEW);
  end;
  --
  procedure TEST_COOKIE(A in varchar2) is
    ACOOKIE owa_cookie.cookie;
  begin
    ACOOKIE := OWA_COOKIE.GET('modowa_cookie');
    OWA_UTIL.MIME_HEADER('text/html', FALSE);
    OWA_COOKIE.SEND('modowa_cookie', 'value_'||A);
    OWA_UTIL.HTTP_HEADER_CLOSE;
    HTP.P('<html><body bgcolor="#ddddee" text="#000000"><p>');
    if (ACOOKIE.NUM_VALS > 0) then
      for I in 1..ACOOKIE.NUM_VALS loop
        HTP.P('modowa_cookie : ['||ACOOKIE.VALS(I)||']<br>');
      end loop;
    else
      HTP.P('modowa_cookie not found');
    end if;
    HTP.P('</p><p><b>Set</b> modowa_cookie : [value_'||A||']</p>');
    HTP.P('</p></body></html>');
  end TEST_COOKIE;
  --
  procedure TEST_AUTH is
    AUTHSTRING varchar2(250);
  begin
    AUTHSTRING := OWA_UTIL.GET_CGI_ENV('HTTP_AUTHORIZATION');
    if (AUTHSTRING is null) then
      OWA_UTIL.MIME_HEADER('text/html', FALSE);
      HTP.PRN('WWW-Authenticate: Basic realm="modowa"');
      OWA_UTIL.HTTP_HEADER_CLOSE;
      return;
    end if;
    OWA_UTIL.MIME_HEADER('text/html', TRUE);
    HTP.P('<html><body bgcolor="#ddddee" text="#000000"><p>');
    HTP.P(AUTHSTRING||'<br>');
    HTP.P(OWA_SEC.GET_USER_ID()||'/'||OWA_SEC.GET_PASSWORD());
    HTP.P('</p></body></html>');
  end TEST_AUTH;
  --
  procedure TEST_BLANK is
  begin
    null;
  end TEST_BLANK;
  --
  procedure TEST_STATUS(ERRCODE in varchar2 default null) is
    ECODE number;
  begin
    if (ERRCODE is not null) then
      ECODE := to_number(ERRCODE);
    else
      ECODE := 500;
    end if;
    OWA_UTIL.STATUS_LINE(ECODE, null, TRUE);
  end TEST_STATUS;
  --
  procedure TEST_RAWARG(R in RAW) is
  begin
    OWA_UTIL.MIME_HEADER('text/plain', TRUE);
    HTP.P('You passed in '||UTL_RAW.CAST_TO_VARCHAR2(R));
  end TEST_RAWARG;
  --
  procedure TEST_RAWBUF(R in RAW) is
  begin
    OWA_UTIL.MIME_HEADER('text/plain', TRUE);
    HTP.P(RAWTOHEX(R));
  end TEST_RAWBUF;
  --
  procedure TEST_PLAIN is
  begin
    OWA_UTIL.MIME_HEADER('text/plain', TRUE);
    HTP.P('The system time is '||to_char(sysdate,'YYYY/MM/DD HH24:MI:SS'));
  end TEST_PLAIN;
  --
  procedure TEST_IMAGE is
  begin
    OWA_UTIL.MIME_HEADER('image/gif', TRUE);
    HTP.PRN(UTL_RAW.CAST_TO_VARCHAR2(IMG_DATA));
  end TEST_IMAGE;
  --
  procedure TEST_XML is
  begin
    OWA_UTIL.MIME_HEADER('text/xml', TRUE);
    HTP.P('<animal><name>Cat</name><sound>Meow</sound></animal>');
  end TEST_XML;
  --
  procedure TEST_DOCLOAD is
    IMG blob;
    LEN integer;
  begin
    LEN := UTL_RAW.LENGTH(IMG_DATA);
    DBMS_LOB.CreateTemporary(IMG, false);
    DBMS_LOB.Write(IMG, LEN, 1, IMG_DATA);
    OWA_UTIL.MIME_HEADER('image/gif', false);
    HTP.P('Content-length: ' || to_char(LEN));
    OWA_UTIL.HTTP_HEADER_CLOSE;
    WPG_DOCLOAD.DOWNLOAD_FILE(IMG);
  end TEST_DOCLOAD;
  --
  procedure TEST_REST(NUM_ENTRIES in number, NAME_ARRAY in VCTAB,
                      VALUE_ARRAY in VCTAB, RESERVED in VCTAB) is
    BLEN        number;
    REST_METHOD varchar2(30);
    CTYPE       varchar2(255);
    FILE_NAME   varchar2(255) := '';
  begin
    REST_METHOD := OWA_UTIL.GET_CGI_ENV('REQUEST_METHOD');

    for I in 1..NUM_ENTRIES loop
      if (NAME_ARRAY(I) = 'MODOWA$CONTENT_BODY') then
        FILE_NAME := VALUE_ARRAY(I);
      end if;
    end loop;

    OWA_UTIL.MIME_HEADER('text/plain', false);
    HTP.P('Method used was '||REST_METHOD||CHR(10));

    if (FILE_NAME is not null) then
      begin
        HTP.P('Content was staged as '||FILE_NAME||CHR(10));
--      select length(BLOB_CONTENT), MIME_TYPE into BLEN, CTYPE
--        from DEMO_DOCLOAD where NAME = FILE_NAME;
--      HTP.P('Type of content was '||CTYPE||chr(10));
--      HTP.P('Length of content was '||to_char(BLEN)||chr(10));
      exception when NO_DATA_FOUND then
        null;
      end;
    end if;
  end TEST_REST;
  --
  procedure TEST_TILDE(R in raw, MTYPE in out varchar2, PBLOB out blob) is
    RLEN   number;
  begin
    RLEN := UTL_RAW.LENGTH(R);
    if (RLEN > 0) then
      MTYPE := 'text/plain';
      DBMS_LOB.CREATETEMPORARY(PBLOB, FALSE);
      DBMS_LOB.OPEN(PBLOB, DBMS_LOB.LOB_READWRITE);
      DBMS_LOB.WRITE(PBLOB, RLEN, 1, R);
      DBMS_LOB.CLOSE(PBLOB);
    else
      MTYPE := 'image/gif';
      DBMS_LOB.CREATETEMPORARY(PBLOB, FALSE);
      DBMS_LOB.OPEN(PBLOB, DBMS_LOB.LOB_READWRITE);
      DBMS_LOB.WRITE(PBLOB, UTL_RAW.LENGTH(IMG_DATA), 1, IMG_DATA);
      DBMS_LOB.CLOSE(PBLOB);
    end if;
  end TEST_TILDE;
  --
  procedure TEST_NCHAR(A in nvarchar2) is
  begin
    OWA_UTIL.MIME_HEADER('text/html', TRUE);
    HTP.P('<html><body bgcolor="white" text="black">');
    HTP.P('<p>Length of NCHAR argument is '||to_char(length(A))||'.</p>');
    HTP.P('</body></html>');
  end TEST_NCHAR;
  --
  procedure TEST_REDIRECT(U in varchar2) is
  begin
    OWA_UTIL.REDIRECT_URL(U);
  end TEST_REDIRECT;
  --
  procedure TEST_NOHEADER(U in varchar2) is
  begin
    HTP.P('<html><body bgcolor=#ffffc0 text=#000000>A');
    for I in 1..60 loop
      HTP.P(U);
    end loop;
    HTP.P('Z</body></html>');
  end TEST_NOHEADER;
  --
  procedure TEST_SCRIPTNAME is
    SCRIPT   varchar2(200);
    PROCNAME varchar2(200);
    IDX      number;
  begin
    OWA_UTIL.MIME_HEADER('text/html', TRUE);
    HTP.P('<html><body bgcolor="#ddddee" text="#000000"><p>');
    SCRIPT := OWA_UTIL.GET_OWA_SERVICE_PATH;
    PROCNAME := OWA_UTIL.GET_PROCEDURE;
    HTP.P('native ['||SCRIPT||'] ['||PROCNAME||']<br>');
    SCRIPT := OWA_UTIL.GET_CGI_ENV('SCRIPT_NAME');
    PROCNAME := OWA_UTIL.GET_CGI_ENV('PATH_INFO');
    IDX := length(SCRIPT);
    HTP.P('original ['||SCRIPT||'] ['||PROCNAME||']<br>');
    if (substr(SCRIPT, IDX, 1) = '/') then
      SCRIPT := substr(SCRIPT, 1, IDX-1);
    end if;
    if (substr(PROCNAME,1,1) <> '/') then
      PROCNAME := '/'||PROCNAME;
    end if;
    HTP.P('adjusted ['||SCRIPT||'] ['||PROCNAME||']<br>');
    while (IDX > 0) loop
      IDX := instr(PROCNAME, '/');
      if (IDX > 0) then -- need to adjust
        SCRIPT := SCRIPT||substr(PROCNAME, 1, IDX);
        PROCNAME := substr(PROCNAME, IDX + 1);
        HTP.P('rewrite ['||SCRIPT||'] ['||PROCNAME||']<br>');
      end if;
    end loop;
    HTP.P('</p></body></html>');
  end TEST_SCRIPTNAME;
  --
  procedure EUROCHARS(N in number default 10) is
    BUF varchar2(30);
  begin
    HTP.P('<html><body bgcolor="#ffffff" text="#000000"><p>');
    BUF :='C'||unistr('\00E1')||'t D'||unistr('\00F6')||'g<br>';
    for I in 1..N loop
      HTP.P(BUF);
    end loop;
    HTP.P('</p></body></html>');
  end EUROCHARS;
  --
  procedure CHINESE(N in number default 10) is
    BUF varchar2(30);
  begin
    HTP.P('<html><body bgcolor="#ffffff" text="#000000"><p>');
    for I in 1..N loop
      HTP.P(i||':'||unistr('\6731\603B\7406')||'<br>');
    end loop;
    HTP.P('</p></body></html>');
  end CHINESE;
  --
  procedure READFILE(NUM_ENTRIES in number, NAME_ARRAY in VCTAB,
                     VALUE_ARRAY in VCTAB, RESERVED in VCTAB,
                     MTYPE out varchar2, PBLOB out blob) is
    LEN integer;
  begin
    LEN := UTL_RAW.LENGTH(IMG_DATA);
    DBMS_LOB.CreateTemporary(PBLOB, false);
    DBMS_LOB.Write(PBLOB, LEN, 1, IMG_DATA);
    MTYPE := 'image/gif';
  end READFILE;
  --
  procedure TEST_CURSOR(NUM_ENTRIES in number, NAME_ARRAY in VCTAB,
                        VALUE_ARRAY in VCTAB, RESERVED in VCTAB,
                        RESULT_CURSOR$ out SYS_REFCURSOR,
                        XML_TAGS$ out varchar2) is
    CTYPE varchar2(250);
  begin
    if (NUM_ENTRIES > 0) then
      CTYPE := VALUE_ARRAY(1);
    else
      OWA_UTIL.MIME_HEADER('text/plain', TRUE);
      HTP.P('No content type specified');
      RESULT_CURSOR$ := null; -- deliberately cause an error
      return;
    end if;
    if (CTYPE = 'text/xml') then
      XML_TAGS$ := 'employees emp hr http://hr.com';
    elsif (CTYPE = 'application/json') then
      XML_TAGS$ := 'employees';
    end if;
    OWA_UTIL.MIME_HEADER(CTYPE, TRUE);
    open RESULT_CURSOR$ for
         select ENAME "Employee Name", SAL "Salary", EMPNO "Employee.Number"
           from (
           select 'John <Jack> Smith' ENAME, 1000 SAL, 1234 EMPNO
             from dual
           union all
           select 'Jane O''Brien' ENAME, 3000 SAL, 5678 EMPNO
             from dual
           union all
           select 'Jones, Alvin' ENAME, 5000 SAL, 9999 EMPNO
             from dual
           union all
           select 'Bob "Buck" Rogers' ENAME, 2000 SAL, null EMPNO
             from dual)
       order by SAL;
  end TEST_CURSOR;
  --
end MODOWA_TEST_PKG;
/
