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
REM Sample alternate implementation of the OWA
REM   A subset of the OWA specification is implemented.  There are the
REM   the most basic APIs from OWA, OWA_SEC, OWA_UTIL, and OWA_COOKIE.
REM   A partial implementation of HTP is also provided.
REM
REM This is only an example - there has been virtually no testing of
REM this code!
REM

create or replace package OWA as
--
  /*
  ** Global types
  */
  type VCTAB  is table of varchar2(32767) index by binary_integer;
  type NMTAB  is table of number index by binary_integer;
  type RAWTAB is table of raw(256) index by binary_integer;
--
  /*
  ** Public package globals
  */
  PROTECTION_REALM  varchar2(255);
  USER_ID           varchar2(255);
  PASSWORD          varchar2(255);
  IP_ADDRESS        NMTAB;
  HOSTNAME          varchar2(255);
--
  function AUTHORIZE return boolean;
--
  /*
  ** Initialize CGI variables
  */
  procedure INIT_CGI_ENV(NPARAMS in number,
                         PNAMES  in VCTAB,
                         PVALS   in VCTAB);
--
  /*
  ** Set username and password
  */
  procedure SET_USER_ID(USR in varchar2);
  procedure SET_PASSWORD(PWD in varchar2);
--
  /*
  ** Return content
  */
  function  GET_LINE(NROWS out integer) return varchar2;
  procedure GET_PAGE(PAGEBUF out VCTAB, NROWS in out integer);
  procedure GET_PAGE_RAW(PAGEBUF out RAWTAB, NROWS in out integer);
  procedure ENABLE_RAW_MODE;
--
  /*
  ** Misc. functions
  */
  procedure SET_CHARSET(IANA_CS in varchar2);
  function  PARSE_COOKIES(NAMES out VCTAB, VALS out VCTAB) return number;
  function  GET_CGI_ENV(PARAM in varchar2) return varchar2;
  procedure PUTRAW(RAWBUF in raw, BUFLEN in number default NULL);
  procedure PUTSTR(CBUF in varchar2);
  procedure PRN(CBUF in varchar2 default null);
  procedure PRINT(CBUF in varchar2 default null);
  procedure INIT;
--
end OWA;
/

create or replace package body OWA as
--
  /*
  ** Private package globals
  */
  NUM_CGI     number := 0;
  CGI_NAMES   VCTAB;
  CGI_VALS    VCTAB;
  --
  NUM_COOKIES number := 0;
  COOK_NAMES  VCTAB;
  COOK_VALS   VCTAB;
  COOKIE_FLAG boolean := false;
  --
  HTBUF       VCTAB;
  HTLASTBUF   varchar2(32767);
  LAST_LEN    number := 0;
  RAW_FLAG    boolean := false;
  HTRAWS      RAWTAB;
  HDR_IDX     number := 0;
  LINES_IN    number := 0;
  LINES_OUT   number := 0;
  CLOSE_FLAG  boolean := false;
  RAW_MAX     constant number := 256;
  HDR_BUF     varchar2(32767);
  --
  NLS_FLAG    boolean := false;
  DB_CHARSET  constant varchar2(30) :=
              substr(userenv('LANGUAGE'), instr(userenv('LANGUAGE'), '.') + 1);
  HT_CHARSET  varchar2(30) := null;
  /* ### Use TO_CHAR(UNISTR('\0010')) in 9i ### */
  NL_CHAR     constant varchar2(10) := convert(CHR(10),DB_CHARSET,'US7ASCII');
--
  function AUTHORIZE return boolean is
  begin
    return(FALSE); /* Prevent direct calls to the routines in this package */
  end AUTHORIZE;
--
  procedure ADJUST_FOR_MULTIBYTE is
  begin
    /*
    ** ### THIS IS A REAL HACK, BUT UNTIL substrb() IS FIXED, IT'S
    ** ### NECESSARY TO ADJUST "HTBUF_LEN" FOR THE DATABASE CHARACTER
    ** ### SET.  THE MAIN PROBLEM WITH THIS CODE IS THAT IT HAS A
    ** ### HARD-CODED LIST OF CHARACTER SETS, WHICH IS BY NO MEANS
    ** ### COMPLETE.  CUSTOMERS WILL HAVE TO EXTEND THIS FOR ANY
    ** ### MULTIBYTE CHARACTER-SET NOT COVERED.
    */
    if (DB_CHARSET = 'UTF8') then
      HTP.HTBUF_LEN := floor(HTP.HTBUF_LEN/3);
    elsif (DB_CHARSET in ('AL32UTF8','ZHT32EUC')) then
      HTP.HTBUF_LEN := floor(HTP.HTBUF_LEN/4);
    elsif (DB_CHARSET in ('JA16EUCYEN','JA16SJIS','JA16SJISYEN',
                          'KO16KSC5601','ZHS16CGB231280','ZHS16GBK',
                          'ZHT16BIG5','JA16MACSJIS','JA16VMS')) then
      HTP.HTBUF_LEN := floor(HTP.HTBUF_LEN/2);
    end if;
  end ADJUST_FOR_MULTIBYTE;
--
  procedure SET_CHARSET(IANA_CS in varchar2) is
    LOWER_CS varchar2(256);
  begin
    LOWER_CS := lower(IANA_CS);
    if (LOWER_CS = 'iso-8859-1')      then  HT_CHARSET := 'WE8ISO8859P1';
    elsif (LOWER_CS = 'utf-8')        then  HT_CHARSET := 'UTF8';
    elsif (LOWER_CS = 'windows-1252') then  HT_CHARSET := 'UTF8';
    elsif (LOWER_CS = 'us-ascii')     then  HT_CHARSET := 'US7ASCII';
    elsif (LOWER_CS = 'iso-8859-2')   then  HT_CHARSET := 'EE8ISO8859P2';
    elsif (LOWER_CS = 'iso-8859-3')   then  HT_CHARSET := 'SE8ISO8859P3';
    elsif (LOWER_CS = 'iso-8859-4')   then  HT_CHARSET := 'NEE8ISO8859P4';
    elsif (LOWER_CS = 'iso-8859-5')   then  HT_CHARSET := 'CL8ISO8859P5';
    elsif (LOWER_CS = 'iso-8859-6')   then  HT_CHARSET := 'AR8ISO8859P6';
    elsif (LOWER_CS = 'iso-8859-7')   then  HT_CHARSET := 'EL8ISO8859P7';
    elsif (LOWER_CS = 'iso-8859-8')   then  HT_CHARSET := 'IW8ISO8859P8';
    elsif (LOWER_CS = 'iso-8859-9')   then  HT_CHARSET := 'WE9ISO8859P9';
    elsif (LOWER_CS = 'iso-8859-10')  then  HT_CHARSET := 'NE8ISO8859P10';
    elsif (LOWER_CS = 'shift-jis')    then  HT_CHARSET := 'JA16SJIS';
    elsif (LOWER_CS = 'gb2312')       then  HT_CHARSET := 'ZHS16CGB231280';
    elsif (LOWER_CS = 'gbk')          then  HT_CHARSET := 'ZHS16GBK';
    elsif (LOWER_CS = 'big5')         then  HT_CHARSET := 'ZHT16BIG5';
    elsif (LOWER_CS = 'euc-tw')       then  HT_CHARSET := 'ZHT32EUC';
    elsif (LOWER_CS = 'tis-620')      then  HT_CHARSET := 'TH8TISASCII';
    elsif (LOWER_CS = 'windows-1256') then  HT_CHARSET := 'AR8MSAWIN';
    elsif (LOWER_CS = 'windows-1257') then  HT_CHARSET := 'BLT8MSWIN1257';
    elsif (LOWER_CS = 'windows-1251') then  HT_CHARSET := 'CL8MSWIN1251';
    elsif (LOWER_CS = 'windows-1250') then  HT_CHARSET := 'EE8MSWIN1250';
    elsif (LOWER_CS = 'windows-1253') then  HT_CHARSET := 'EL8MSWIN1253';
    elsif (LOWER_CS = 'windows-1255') then  HT_CHARSET := 'IW8MSWIN1255';
    elsif (LOWER_CS = 'windows-1254') then  HT_CHARSET := 'TR8MSWIN1254';
    elsif (LOWER_CS = 'windows-1258') then  HT_CHARSET := 'VN8MSWIN1258';
    elsif (LOWER_CS = 'windows-921')  then  HT_CHARSET := 'LT8MSWIN921';
    else
      HT_CHARSET := DB_CHARSET;
    end if;
  end SET_CHARSET;
--
  procedure INIT_CGI_ENV(NPARAMS in number,
                         PNAMES  in VCTAB,
                         PVALS   in VCTAB) is
  begin
    NUM_CGI := NPARAMS;
    CGI_VALS := PVALS;
    for I in 1..NUM_CGI loop
      CGI_NAMES(I) := upper(PNAMES(I));
    end loop;
    INIT;
  end INIT_CGI_ENV;
--
  procedure SET_USER_ID(USR in varchar2) is
  begin
    USER_ID := USR;
  end SET_USER_ID;
--
  procedure SET_PASSWORD(PWD in varchar2) is
  begin
    PASSWORD := PWD;
  end SET_PASSWORD;
--
  procedure PUT(CBUF in varchar2) is
    LOC pls_integer;
    LEN pls_integer;
  begin
    if (CBUF is not null) then
      LEN := length(CBUF);
      if (HDR_IDX > 0) then
        LOC := HTP.HTBUF_LEN - LAST_LEN;
        if (LOC > LEN) then
          LOC := LEN;
        end if;
        if (LOC > 0) then
          HTLASTBUF := HTLASTBUF||substr(CBUF, 1, LOC);
          LAST_LEN := LAST_LEN + LOC;
        end if;
      else
        LOC := 0;
      end if;
      while (LOC < LEN) loop
        HTBUF(LINES_IN) := HTLASTBUF;
        LINES_IN := LINES_IN + 1;
        HTLASTBUF := substr(CBUF, LOC + 1, HTP.HTBUF_LEN);
        LAST_LEN := LEN - LOC;
        LOC := LOC + HTP.HTBUF_LEN;
      end loop;
    end if;
  end PUT;
--
  procedure MARK_HEADER is
  begin
    if (not RAW_FLAG) then
      HTBUF(LINES_IN) := HTLASTBUF;
      HTLASTBUF := '';
      HDR_IDX := LINES_IN;
      LINES_IN := LINES_IN + 1;
    else
      HDR_IDX := LINES_IN;
    end if;
    LAST_LEN := 0;
  end MARK_HEADER;
--
  procedure CLOSE_PAGE is
    LEN pls_integer;
    LOC pls_integer;
    BUF varchar2(255);
  begin
    if (CLOSE_FLAG) then
      return;
    end if;
    if ((HDR_IDX = 0) and (HDR_BUF is not null)) then
      if (RAW_FLAG) then
        PUTSTR(HDR_BUF);
      else
        PUT(HDR_BUF);
      end if;
    end if;
    if ((HTLASTBUF is not null) or (not HTBUF.exists(LINES_IN))) then
      HTBUF(LINES_IN) := HTLASTBUF;
    end if;
    if (LINES_IN > 1) then
      /*
      ** ### WHY BOTHER COMPUTING THE Content-Length HERE?  IT WILL BE
      ** ### WRONG ANYWAY IF A CHARACTER SET CONVERSION OCCURS BETWEEN
      ** ### PL/SQL AND Apache, AND mod_owa WILL RECOMPUTE IT ANYWAY
      ** ### IF YOUR PAGE IS SMALL ENOUGH (LESS THAN ABOUT 200K).
      */
      BUF := '';
      if (HDR_IDX > 0) then
        LOC := HDR_IDX + 1;
        if (RAW_FLAG) then
          LEN := LAST_LEN;
          if (LINES_IN > LOC) then
            LEN := LEN + ((LINES_IN - LOC) * RAW_MAX);
          end if;
        else
          LEN := 0;
          for I in LOC..LINES_IN loop
            LEN := LEN + lengthb(HTBUF(I));
          end loop;
        end if;
        BUF := 'Content-Length: '||to_char(LEN)||NL_CHAR;
      end if;
      if (not RAW_FLAG) then
        HTBUF(1) := BUF;
      elsif (HT_CHARSET = DB_CHARSET) then
        HTRAWS(1) := UTL_RAW.CAST_TO_RAW(BUF);
      else
        HTRAWS(1) := UTL_RAW.CONVERT(UTL_RAW.CAST_TO_RAW(BUF),
                                     'AMERICAN_AMERICA.'||HT_CHARSET,
                                     'AMERICAN_AMERICA.'||DB_CHARSET);
      end if;
    end if;
    CLOSE_FLAG := true;
  end CLOSE_PAGE;
--
  function GET_LINE(NROWS out integer) return varchar2 is
    NLINES number;
  begin
    CLOSE_PAGE;
    NLINES := LINES_IN - LINES_OUT;
    if (NLINES > 1) then
      NROWS := 1;
    else
      NROWS := 0;
      if (NLINES < 1) then
        return(NULL);
      end if;
    end if;
    LINES_OUT := LINES_OUT + 1;
    return(HTBUF(LINES_OUT));
  end GET_LINE;
--
  procedure GET_PAGE(PAGEBUF out VCTAB, NROWS in out integer) is
  begin
    CLOSE_PAGE;
    NROWS := least(NROWS, LINES_IN - LINES_OUT);
    if (NROWS > 0) then
      for I in 1..NROWS loop
        PAGEBUF(I) := HTBUF(LINES_OUT + I);
      end loop;
      LINES_OUT := LINES_OUT + NROWS;
    end if;
  end GET_PAGE;
--
  procedure GET_PAGE_RAW(PAGEBUF out RAWTAB, NROWS in out integer) is
  begin
    CLOSE_PAGE;
    NROWS := least(NROWS, LINES_IN - LINES_OUT);
    if (NROWS > 0) then
      for I in 1..NROWS loop
        PAGEBUF(I) := HTRAWS(LINES_OUT + I);
      end loop;
      LINES_OUT := LINES_OUT + NROWS;
    end if;
  end GET_PAGE_RAW;
--
  procedure ENABLE_RAW_MODE is
  begin
    RAW_FLAG := true;
  end ENABLE_RAW_MODE;
--
  function GET_CGI_ENV(PARAM in varchar2) return varchar2 is
    UPARAM varchar2(4000);
  begin
    UPARAM := upper(PARAM);
    for I in 1..NUM_CGI loop
      if (CGI_NAMES(I) = UPARAM) then
        return(CGI_VALS(I));
      end if;
    end loop;
    return(null);
  end GET_CGI_ENV;
--
  function PARSE_COOKIES(NAMES out VCTAB, VALS out VCTAB) return number is
    BUFFER    varchar2(32767);
    START_LOC pls_integer;
    END_LOC   pls_integer;
    EQ_LOC    pls_integer;
  begin
    if (COOKIE_FLAG) then
      NAMES := COOK_NAMES;
      VALS := COOK_VALS;
      return(NUM_COOKIES);
    end if;
    COOKIE_FLAG := true;
    BUFFER := GET_CGI_ENV('HTTP_COOKIE');
    NUM_COOKIES := 0;
    if (BUFFER is null) then
      return(NUM_COOKIES);
    end if;
    END_LOC := length(BUFFER);
    if (substr(BUFFER, END_LOC, 1) <> ';') then
      BUFFER := BUFFER||';';
    end if;
    START_LOC := 1;
    while (true) loop
      END_LOC := instr(BUFFER, ';', START_LOC);
      EQ_LOC := instr(BUFFER, '=', START_LOC);
      if (EQ_LOC = 0) then
        exit;
      end if;
      NUM_COOKIES := NUM_COOKIES + 1;
      COOK_NAMES(NUM_COOKIES) :=
        ltrim(substr(BUFFER, START_LOC, EQ_LOC-START_LOC));
      COOK_VALS(NUM_COOKIES) := substr(BUFFER, EQ_LOC+1, END_LOC-EQ_LOC-1);
      START_LOC := END_LOC + 1;
      END_LOC := instr(BUFFER, ';', START_LOC);
    end loop;
    NAMES := COOK_NAMES;
    VALS := COOK_VALS;
    return(NUM_COOKIES);
  end PARSE_COOKIES;
--
  procedure PUTRAW(RAWBUF in raw, BUFLEN in number default NULL) is
    BLEN binary_integer;
    BLOC binary_integer;
    BCPY binary_integer;
  begin
    if (RAWBUF is not null) then
      if (BUFLEN is not null) then
        BLEN := BUFLEN;
      else
        BLEN := UTL_RAW.LENGTH(RAWBUF);
      end if;
      if (BLEN > 0) then
        BLOC := 1;
        if (LAST_LEN = 0) then
           LAST_LEN := RAW_MAX;
        end if;
        while (bloc <= blen) loop
          if (LAST_LEN = RAW_MAX) then
            LINES_IN := LINES_IN + 1;
            BCPY := least((BLEN - BLOC) + 1, RAW_MAX);
            HTRAWS(LINES_IN) := UTL_RAW.SUBSTR(RAWBUF, BLOC, BCPY);
            LAST_LEN := BCPY;
          else
            BCPY := least((BLEN - BLOC) + 1, RAW_MAX - LAST_LEN);
            HTRAWS(LINES_IN) := UTL_RAW.CONCAT(HTRAWS(LINES_IN),
                                             UTL_RAW.SUBSTR(RAWBUF,BLOC,BCPY));
            LAST_LEN := LAST_LEN + BCPY;
          end if;
          BLOC := BLOC + BCPY;
        end loop;
      end if;
    end if;
  end PUTRAW;
--
  procedure PUTSTR(CBUF in varchar2) is
  begin
    if (LINES_IN = 0) then
      LINES_IN := LINES_IN + 1; -- reserve space for Content-Length
      HTBUF(1) := null;
      HTRAWS(1) := null;
    end if;
    if (HT_CHARSET = DB_CHARSET) then
      PUTRAW(UTL_RAW.CAST_TO_RAW(CBUF));
    else
      PUTRAW(UTL_RAW.CONVERT(UTL_RAW.CAST_TO_RAW(CBUF),
                             'AMERICAN_AMERICA.'||HT_CHARSET,
                             'AMERICAN_AMERICA.'||DB_CHARSET));
    end if;
  end PUTSTR;
--
  function IS_HTTP_HEADER(STR in varchar2, LEN in number)
    return boolean is
    ALNUM varchar2(250) :=
      'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-0123456789';
    /* These are also allowed: "!#$%&'*+.^_`~" */
    IDX   pls_integer;
    CH    varchar2(30);
  begin
    IDX := 1;
    while (instr(ALNUM, substr(STR, IDX, 1)) > 0) loop
      if (IDX = LEN) then
        return(false);
      end if;
      IDX := IDX + 1;
    end loop;
    while (substr(STR, IDX, 1) = ' ') loop
      if (IDX = LEN) then
        return(false);
      end if;
      IDX := IDX + 1;
    end loop;
    return(substr(STR, IDX, 1) = ':');
  end IS_HTTP_HEADER;
--
  procedure PUT_HEADER(CBUF in varchar2) is
    LOC pls_integer;
  begin
    -- Scan for the newline that separates the header from the content
    HDR_BUF := HDR_BUF||CBUF;
    while ((HDR_BUF is not null) and (HDR_IDX = 0)) loop
      LOC := instr(HDR_BUF, NL_CHAR);
      if (LOC = 0) then
        if (length(HDR_BUF) < 2000) then
          exit;
        else
          LOC := 2000;
        end if;
      elsif ((LOC > 1) and (not IS_HTTP_HEADER(HDR_BUF, LOC))) then
        if (RAW_FLAG) then
          PUTSTR(NL_CHAR); -- ### no Content-Type
        else
          PUT(NL_CHAR); -- ### no Content-Type
        end if;
        MARK_HEADER;
      end if;
      if (RAW_FLAG) then
        PUTSTR(substr(HDR_BUF, 1, LOC));
      else
        PUT(substr(HDR_BUF, 1, LOC));
      end if;
      if (LOC = 1) then
        MARK_HEADER;
      end if;
      HDR_BUF := substr(HDR_BUF, LOC + 1);
    end loop;
    if ((HDR_IDX > 0) and (HDR_BUF is not null)) then
      if (RAW_FLAG) then
        PUTSTR(HDR_BUF);
      else
        PUT(HDR_BUF);
      end if;
    end if;
  end PUT_HEADER;
--
  procedure PRN(CBUF in varchar2 default null) is
    LOC pls_integer;
    LEN pls_integer;
  begin
    if (LINES_IN = 0) then
      LINES_IN := 1; -- reserve space for Content-Length
      HTBUF(1) := null;
      HTRAWS(1) := null;
    end if;
    if (HDR_IDX = 0) then
      PUT_HEADER(CBUF);
    elsif (RAW_FLAG) then
      PUTSTR(CBUF);
    else
      PUT(CBUF);
    end if;
  end PRN;
--
  procedure PRINT(CBUF in varchar2 default null) is
  begin
    PRN(CBUF||NL_CHAR);
  end PRINT;
--
  procedure INIT is
  begin
    LINES_IN := 0;
    LINES_OUT := 0;
    COOKIE_FLAG := false;
    NUM_COOKIES := 0;
    if (not NLS_FLAG) then
      ADJUST_FOR_MULTIBYTE;
      NLS_FLAG := true;
    end if;
    RAW_FLAG := false;
    HT_CHARSET := DB_CHARSET;
    HDR_IDX := 0;
    LAST_LEN := 0;
    CLOSE_FLAG := false;
    HDR_BUF := '';
    HTLASTBUF := '';
    if (HTBUF is not null) then
      HTBUF.delete;
    end if;
    if (HTRAWS is not null) then
      HTRAWS.delete;
    end if;
  end INIT;
--
begin
  INIT;
end OWA;
/

create or replace package OWA_CUSTOM as
  function AUTHORIZE return boolean;
end OWA_CUSTOM;
/

create or replace package OWA_INIT as
  function AUTHORIZE return boolean;
end OWA_INIT;
/

create or replace package body OWA_CUSTOM as
--
  function AUTHORIZE return boolean is
  begin
    return(TRUE);
  end;
--
end OWA_CUSTOM;
/

create or replace package body OWA_INIT as
--
  function AUTHORIZE return boolean is
  begin
    return(TRUE);
  end;
--
end OWA_INIT;
/

create or replace package OWA_SEC as
  function GET_USER_ID return varchar2;
  function GET_PASSWORD return varchar2;
  procedure SET_PROTECTION_REALM(REALM in varchar2);
end OWA_SEC;
/

create or replace package OWA_UTIL as
--
  type IDENT_ARR is table of varchar2(30) index by binary_integer;
  type IDENT_NARR is table of nvarchar2(30) index by binary_integer;
  type NUM_ARR is table of number index by binary_integer;
  type IP_ADDRESS is table of integer index by binary_integer;
  type VC_ARR is table of varchar2(32000) index by binary_integer;
  type NC_ARR is table of nvarchar2(16000) index by binary_integer;
  type RAW_ARR is table of raw(32000) index by binary_integer;
--
  function GET_CGI_ENV(PARAM in varchar2) return varchar2;
  function GET_OWA_SERVICE_PATH return varchar2;
  function GET_PROCEDURE return varchar2;
  procedure MIME_HEADER(MIME_TYPE  in varchar2 default null,
                        CLOSE_FLAG in boolean default true,
                        CHAR_SET   in varchar2 default null /* Magic? */);
  procedure REDIRECT_URL(TARGET_URL in varchar2,
                         CLOSE_FLAG in boolean default true);
  procedure STATUS_LINE(STATUS     in integer,
                        REASON     in varchar2 default null,
                        CLOSE_FLAG in boolean default true);
  procedure HTTP_HEADER_CLOSE;
--
end OWA_UTIL;
/

create or replace package OWA_COOKIE as
  --
  type COOKIE is record
  (
    NAME     varchar2(4096),
    VALS     OWA.VCTAB,
    NUM_VALS integer
  );
  --
  procedure SEND(NAME    in varchar2,
                 VALUE   in varchar2,
                 EXPIRES in date     default null,
                 PATH    in varchar2 default null,
                 DOMAIN  in varchar2 default null,
                 SECURE  in varchar2 default null);
  function GET(NAME in varchar2) return COOKIE;
  procedure REMOVE(NAME in varchar2,
                   VAL  in varchar2,
                   PATH in varchar2 default null);
  procedure GET_ALL(NAMES    out OWA.VCTAB,
                    VALS     out OWA.VCTAB,
                    NUM_VALS out integer);
end OWA_COOKIE;
/

create or replace package body OWA_SEC as
--
  function GET_USER_ID return varchar2 is
  begin
    return(OWA.USER_ID);
  end GET_USER_ID;
--
  function GET_PASSWORD return varchar2 is
  begin
    return(OWA.PASSWORD);
  end GET_PASSWORD;
--
  procedure SET_PROTECTION_REALM(REALM in varchar2) is
  begin
    OWA.PROTECTION_REALM := REALM;
  end SET_PROTECTION_REALM;
--
end OWA_SEC;
/

create or replace package body OWA_UTIL as
--
  function GET_CGI_ENV(PARAM in varchar2) return varchar2 is
  begin
    return(OWA.GET_CGI_ENV(PARAM));
  end GET_CGI_ENV;
--
  function GET_OWA_SERVICE_PATH return varchar2 is
    SCRIPT_NAME varchar2(4000);
  begin
    SCRIPT_NAME := OWA.GET_CGI_ENV('SCRIPT_NAME');
    if (SCRIPT_NAME is not null) then
      if (substr(SCRIPT_NAME, length(SCRIPT_NAME) - 1) <> '/') then
        return(SCRIPT_NAME||'/');
      end if;
    end if;
    return(SCRIPT_NAME);
  end GET_OWA_SERVICE_PATH;
--
  function RESOLVE_NAME(PNAME in varchar2) return varchar2 is
    PART1   varchar2(100);
    PART2   varchar2(100);
    OWNER   varchar2(100);
    DBLINK  varchar2(100);
    RNAME   varchar2(100);
    PTYPE   number;
    OBJNUM  number;
  begin
    /*
    ** ### THIS CODE IS WRONG, BUT KEPT FOR REFERENCE.  THE PROBLEM
    ** ### IS THAT THIS RUNS AS THE INSTALLATION USER FOR THE OWA.
    ** ### THUS, IT WILL WORK CORRECTLY ONLY FOR PUBLIC SYNONYMS OR
    ** ### IF THE APPLICATION IS INSTALLED IN THE SAME SCHEMA AS
    ** ### THE OWA.  NEED TO RUN THIS FUNCTION WITH INVOKER'S RIGHTS.
    */
    DBMS_UTILITY.NAME_RESOLVE(PNAME, 1, OWNER, PART1, PART2,
                              DBLINK, PTYPE, OBJNUM);
    if (PART2 is null) then
      RNAME := PART1;
    elsif (PTYPE in (7,8)) then
      RNAME := PART2;
    else
      RNAME := PART1||'.'||PART2;
    end if;
    return(RNAME);
  exception when OTHERS then
    return(upper(PNAME));
  end RESOLVE_NAME;
--
  function GET_PROCEDURE return varchar2 is
    PATH_INFO  varchar2(4000);
  begin
    PATH_INFO := OWA.GET_CGI_ENV('PATH_INFO');
    if (substr(PATH_INFO, 1, 1) = '/') then
      PATH_INFO := substr(PATH_INFO, 2);
    end if;
    -- ### SHOULD USE RESOLVE_NAME() BUT DOESN'T WORK YET
    return(upper(PATH_INFO));
  end GET_PROCEDURE;
--
  procedure MIME_HEADER(MIME_TYPE  in varchar2 default null,
                        CLOSE_FLAG in boolean  default true,
                        CHAR_SET   in varchar2 default null /* Magic? */) is
    CHARSET  varchar2(4000);
    MIMETYPE varchar2(4000);
  begin
    if (MIME_TYPE is null) then
      MIMETYPE := 'text/html';
    else
      MIMETYPE := MIME_TYPE;
    end if;
    if (CHAR_SET is null) then
      if (upper(MIMETYPE) like 'TEXT/%') then
        CHARSET := GET_CGI_ENV('REQUEST_IANA_CHARSET');
      else
        CHARSET := CHAR_SET;
      end if;
    end if;
    if (CHARSET is null) then
      OWA.PRINT('Content-Type: '||MIMETYPE);
    else
      OWA.SET_CHARSET(CHARSET);
      OWA.PRINT('Content-Type: '||MIMETYPE||'; charset='||CHARSET);
    end if;
    if (CLOSE_FLAG) then
      HTTP_HEADER_CLOSE;
    end if;
  end MIME_HEADER;
--
  procedure REDIRECT_URL(TARGET_URL in varchar2,
                         CLOSE_FLAG in boolean default true) is
  begin
    OWA.PRINT('Location: '||TARGET_URL);
    if (CLOSE_FLAG) then
      HTTP_HEADER_CLOSE;
    end if;
  end REDIRECT_URL;
--
  procedure STATUS_LINE(STATUS     in integer,
                        REASON     in varchar2 default null,
                        CLOSE_FLAG in boolean default true) is
  begin
    OWA.PRINT('Status: '||STATUS||' '||REASON);
    if (CLOSE_FLAG) then
      HTTP_HEADER_CLOSE;
    end if;
  end STATUS_LINE;
--
  procedure HTTP_HEADER_CLOSE is
  begin
    OWA.PRINT('');
  end HTTP_HEADER_CLOSE;
--
end OWA_UTIL;
/

create or replace package body OWA_COOKIE as
--
  procedure SEND(NAME    in varchar2,
                 VALUE   in varchar2,
                 EXPIRES in date     default null,
                 PATH    in varchar2 default null,
                 DOMAIN  in varchar2 default null,
                 SECURE  in varchar2 default null) is
    COOKIE_LINE varchar2(4000);
    MINUTES     number := 0;
    T           date;
  begin
    COOKIE_LINE := 'Set-Cookie: '||NAME||'='||VALUE||';';
    if (EXPIRES is not null) then
      -- Convert EXPIRES to GMT
      /* ### WORKS ONLY WITH ORACLE 9i ###
      MINUTES := to_number(to_char(SYSTIMESTAMP,'TZH') * 60);
      if (MINUTES < 0) then
        MINUTES := MINUTES - to_number(to_char(SYSTIMESTAMP,'TZM'));
      else
        MINUTES := MINUTES + to_number(to_char(SYSTIMESTAMP,'TZM'));
      end if;
         ### WORKS ONLY WITH ORACLE 9i ### */
      if (MINUTES <> 0) then
        T := EXPIRES - MINUTES/(24*60);
      else
        T := EXPIRES;
      end if;
      COOKIE_LINE := COOKIE_LINE||' expires='||
                     rtrim(to_char(T, 'Day',
                                  'NLS_DATE_LANGUAGE=AMERICAN'), ' ')||
                     ', '||to_char(T, 'DD-Mon-YYYY HH24:MI:SS',
                                   'NLS_DATE_LANGUAGE=AMERICAN')||' GMT;';
    end if;
    if (PATH is not null) then
      COOKIE_LINE := COOKIE_LINE||' path='||PATH||';';
    end if;
    if (DOMAIN is not null) then
      COOKIE_LINE := COOKIE_LINE||' domain='||DOMAIN||';';
    end if;
    if (SECURE is not null) then
      COOKIE_LINE := COOKIE_LINE||' secure';
    end if;
    OWA.PRINT(COOKIE_LINE);
  end SEND;
--
  function GET(NAME in varchar2) return COOKIE is
    ACOOKIE     COOKIE;
    NUM_COOKIES number;
    NAMES       OWA.VCTAB;
    VALS        OWA.VCTAB;
  begin
    NUM_COOKIES := OWA.PARSE_COOKIES(NAMES, VALS);
    ACOOKIE.NUM_VALS := 0;
    ACOOKIE.NAME := NAME;
    for I in 1..NUM_COOKIES loop
      if (NAMES(I) = NAME) then
        ACOOKIE.NUM_VALS := ACOOKIE.NUM_VALS + 1;
        ACOOKIE.VALS(ACOOKIE.NUM_VALS) := VALS(I);
      end if;
    end loop;
    return(ACOOKIE);
  end GET;
--
  procedure REMOVE(NAME in varchar2,
                   VAL  in varchar2,
                   PATH in varchar2 default null) is
  begin
    SEND(NAME, VAL, to_date('YYYY/MM/DD','1970/01/01'));
  end REMOVE;
--
  procedure GET_ALL(NAMES    out OWA.VCTAB,
                    VALS     out OWA.VCTAB,
                    NUM_VALS out integer) is
  begin
    NUM_VALS := OWA.PARSE_COOKIES(NAMES, VALS);
  end GET_ALL;
--
end OWA_COOKIE;
/

create or replace package HTP as
  /* Modifiable package global */
  HTBUF_LEN number := 255; -- ### DOESN'T WORK FOR MULTIBYTE
--
  procedure PRN(CBUF in varchar2 default null);
  procedure PRN(DVAL in date);
  procedure PRN(NVAL in number);
  procedure PRINT(CBUF in varchar2 default null);
  procedure PRINT(DVAL in date);
  procedure PRINT(NVAL in number);
  procedure P(CBUF in varchar2 default null);
  procedure P(DVAL in date);
  procedure P(NVAL in number);
--
  function ESCAPE_ARG(ARG in varchar2) return varchar2;
  function ESCAPE_SC(CTEXT in varchar2) return varchar2;
  function ESCAPE_URL(URL in varchar2) return varchar2;
--
  procedure HTMLOPEN;
  procedure HEADOPEN;
  procedure BODYOPEN(CBACKGROUND in varchar2 default null,
                     CATTRIBUTES in varchar2 default null);
  procedure FRAMESETOPEN(CROWS       in varchar2 default null,
                         CCOLS       in varchar2 default null,
                         CATTRIBUTES in varchar2 default null);
  procedure MAPOPEN(CNAME in varchar2,
                    CATTRIBUTES in varchar2 default null);
  procedure NOFRAMESOPEN;
  procedure CENTEROPEN;
  procedure LISTINGOPEN;
  procedure PREOPEN(CCLEAR      in varchar2 default null,
                    CWIDTH      in varchar2 default null,
                    CATTRIBUTES in varchar2 default null);
  procedure BLOCKQUOTEOPEN(CNOWRAP     in varchar2 default null,
                           CCLEAR      in varchar2 default null,
                           CATTRIBUTES in varchar2 default null);
  procedure DIRLISTOPEN;
  procedure MENULISTOPEN;
  procedure ULISTOPEN(CCLEAR      in varchar2 default null,
                      CWRAP       in varchar2 default null,
                      CDINGBAT    in varchar2 default null,
                      CSRC        in varchar2 default null,
                      CATTRIBUTES in varchar2 default null);
  procedure OLISTOPEN(CCLEAR      in varchar2 default null,
                      CWRAP       in varchar2 default null,
                      CATTRIBUTES in varchar2 default null);
  procedure DLISTOPEN(CCLEAR      in varchar2 default null,
                      CATTRIBUTES in varchar2 default null);
  procedure HTMLCLOSE;
  procedure HEADCLOSE;
  procedure BODYCLOSE;
  procedure FRAMESETCLOSE;
  procedure MAPCLOSE;
  procedure NOFRAMESCLOSE;
  procedure CENTERCLOSE;
  procedure LISTINGCLOSE;
  procedure PRECLOSE;
  procedure BLOCKQUOTECLOSE;
  procedure DIRLISTCLOSE;
  procedure MENULISTCLOSE;
  procedure ULISTCLOSE;
  procedure OLISTCLOSE;
  procedure DLISTCLOSE;
--
  procedure TITLE(CTITLE in varchar);
  procedure HR(CCLEAR      in varchar2 default null,
               CSRC        in varchar2 default null,
               CATTRIBUTES in varchar2 default null);
  procedure LINE(CCLEAR      in varchar2 default null,
                 CSRC        in varchar2 default null,
                 CATTRIBUTES in varchar2 default null);
  procedure BR(CCLEAR      in varchar2 default null,
               CATTRIBUTES in varchar2 default null);
  procedure NL(CCLEAR      in varchar2 default null,
               CATTRIBUTES in varchar2 default null);
  procedure IMG2(CSRC        in varchar2,
                 CALIGN      in varchar2 default null,
                 CALT        in varchar2 default null,
                 CISMAP      in varchar2 default null,
                 CUSEMAP     in varchar2 default null,
                 CATTRIBUTES in varchar2 default null);
  procedure IMG(CSRC        in varchar2,
                CALIGN      in varchar2 default null,
                CALT        in varchar2 default null,
                CISMAP      in varchar2 default null,
                CATTRIBUTES in varchar2 default null);
  procedure AREA(CCOORDS     in varchar2,
                 CSHAPE      in varchar2 default null,
                 CHREF       in varchar2 default null,
                 CNOHREF     in varchar2 default null,
                 CTARGET     in varchar2 default null,
                 CATTRIBUTES in varchar2 default null);
  procedure FRAME(CSRC          in varchar2,
                  CNAME         in varchar2 default null,
                  CMARGINWIDTH  in varchar2 default null,
                  CMARGINHEIGHT in varchar2 default null,
                  CSCROLLING    in varchar2 default null,
                  CNORESIZE     in varchar2 default null,
                  CATTRIBUTES   in varchar2 default null);
  procedure HEADER(NSIZE       in integer,
                   CTEXT       in varchar2,
                   CALIGN      in varchar2 default null,
                   CNOWRAP     in varchar2 default null,
                   CCLEAR      in varchar2 default null,
                   CATTRIBUTES in varchar2 default null);
  procedure HTITLE(CTITLE      in varchar2,
                   NSIZE       in integer default 1,
                   CALIGN      in varchar2 default null,
                   CNOWRAP     in varchar2 default null,
                   CCLEAR      in varchar2 default null,
                   CATTRIBUTES in varchar2 default null);
  procedure ANCHOR2(CURL        in varchar2,
                    CTEXT       in varchar2, 
                    CNAME       in varchar2 default null,
                    CTARGET     in varchar2 default null,
                    CATTRIBUTES in varchar2 default null);
  procedure ANCHOR(CURL        in varchar2,
                   CTEXT       in varchar2, 
                   CNAME       in varchar2 default null,
                   CATTRIBUTES in varchar2 default null);
  procedure LISTHEADER(CTEXT       in varchar2,
                       CATTRIBUTES in varchar2 default null);
  procedure LISTITEM(CTEXT   in varchar2 default null,
                     CCLEAR   in varchar2 default null,
                     CDINGBAT in varchar2 default null,
                     CSRC     in varchar2 default null,
                     CATTRIBUTES in varchar2 default null);
  procedure DLISTTERM(CTEXT in varchar2 default null,
                      CCLEAR in varchar2 default null,
                      CATTRIBUTES in varchar2 default null);
  procedure DLISTDEF(CTEXT in varchar2 default null,
                     CCLEAR in varchar2 default null,
                     CATTRIBUTES in varchar2 default null);
--
  procedure PARA;
  procedure PARAGRAPH(CALIGN      in varchar2 default null,
                      CNOWRAP     in varchar2 default null,
                      CCLEAR      in varchar2 default null,
                      CATTRIBUTES in varchar2 default null);
  procedure DIV(CALIGN      in varchar2 default null,
                CATTRIBUTES in varchar2 default null);
--
  procedure WBR;
  procedure COMMENT(CTEXT in varchar2);
  procedure NOBR(CTEXT in varchar2);
  procedure CENTER(CTEXT in varchar2);
--
  procedure DFN(CTEXT in varchar2,
                CATTRIBUTES in varchar2 default null);
  procedure CITE(CTEXT in varchar2,
                 CATTRIBUTES in varchar2 default null);
  procedure CODE(CTEXT in varchar2,
                 CATTRIBUTES in varchar2 default null);
  procedure EM(CTEXT in varchar2,
               CATTRIBUTES in varchar2 default null);
  procedure EMPHASIS(CTEXT in varchar2,
                     CATTRIBUTES in varchar2 default null);
  procedure KBD(CTEXT in varchar2,
                CATTRIBUTES in varchar2 default null);
  procedure KEYBOARD(CTEXT in varchar2,
                     CATTRIBUTES in varchar2 default null);
  procedure SAMPLE(CTEXT in varchar2,
                   CATTRIBUTES in varchar2 default null);
  procedure VARIABLE(CTEXT in varchar2,
                     CATTRIBUTES in varchar2 default null);
  procedure STRONG(CTEXT in varchar2,
                   CATTRIBUTES in varchar2 default null);
  procedure BIG(CTEXT in varchar2,
                CATTRIBUTES in varchar2 default null);
  procedure SMALL(CTEXT in varchar2,
                  CATTRIBUTES in varchar2 default null);
  procedure BOLD(CTEXT in varchar2,
                 CATTRIBUTES in varchar2 default null);
  procedure ITALIC(CTEXT in varchar2,
                   CATTRIBUTES in varchar2 default null);
  procedure UNDERLINE(CTEXT in varchar2,
                      CATTRIBUTES in varchar2 default null);
  procedure PLAINTEXT(CTEXT in varchar2,
                      CATTRIBUTES in varchar2 default null);
  procedure TELETYPE(CTEXT in varchar2,
                     CATTRIBUTES in varchar2 default null);
  procedure S(CTEXT in varchar2,
              CATTRIBUTES in varchar2 default null);
  procedure STRIKE(CTEXT in varchar2,
                   CATTRIBUTES in varchar2 default null);
  procedure SUB(CTEXT       in varchar2,
                CALIGN      in varchar2 default null,
                CATTRIBUTES in varchar2 default null);
  procedure SUP(CTEXT       in varchar2,
                CALIGN      in varchar2 default null,
                CATTRIBUTES in varchar2 default null);
--
  procedure TABLEOPEN(CBORDER     in varchar2 default null,
                      CALIGN      in varchar2 default null,
                      CNOWRAP     in varchar2 default null,
                      CCLEAR      in varchar2 default null,
                      CATTRIBUTES in varchar2 default null);
  procedure TABLECAPTION(CCAPTION    in varchar2,
                         CALIGN      in varchar2 default null,
                         CATTRIBUTES in varchar2 default null);
  procedure TABLEROWOPEN(CALIGN      in varchar2 default null,
                         CVALIGN     in varchar2 default null,
                         CDP         in varchar2 default null,
                         CNOWRAP     in varchar2 default null,
                         CATTRIBUTES in varchar2 default null);
  procedure TABLEHEADER(CVALUE      in varchar2 default null,
                        CALIGN      in varchar2 default null,
                        CDP         in varchar2 default null,
                        CNOWRAP     in varchar2 default null,
                        CROWSPAN    in varchar2 default null,
                        CCOLSPAN    in varchar2 default null,
                        CATTRIBUTES in varchar2 default null);
  procedure TABLEDATA(CVALUE      in varchar2 default null,
                      CALIGN      in varchar2 default null,
                      CDP         in varchar2 default null,
                      CNOWRAP     in varchar2 default null,
                      CROWSPAN    in varchar2 default null,
                      CCOLSPAN    in varchar2 default null,
                      CATTRIBUTES in varchar2 default null);
  procedure TABLEROWCLOSE;
  procedure TABLECLOSE;
--
  procedure FORMOPEN(CURL        in varchar2,
                     CMETHOD     in varchar2 default 'POST',
                     CTARGET     in varchar2 default null,
                     CENCTYPE    in varchar2 default null, 
                     CATTRIBUTES in varchar2 default null);
  procedure FORMCHECKBOX(CNAME       in varchar2,
                         CVALUE      in varchar2 default 'on',
                         CCHECKED    in varchar2 default null,
                         CATTRIBUTES in varchar2 default null);
  procedure FORMFILE(CNAME       in varchar2,
                     CACCEPT     in varchar2 default null,
                     CATTRIBUTES in varchar2 default null);
  procedure FORMHIDDEN(CNAME       in varchar2,
                       CVALUE      in varchar2 default null,
                       CATTRIBUTES in varchar2 default null);
  procedure FORMIMAGE(CNAME       in varchar2,
                      CSRC        in varchar2,
                      CALIGN      in varchar2 default null,
                      CATTRIBUTES in varchar2 default null);
  procedure FORMPASSWORD(CNAME       in varchar2,
                         CSIZE       in varchar2 default null,
                         CMAXLENGTH  in varchar2 default null,
                         CVALUE      in varchar2 default null,
                         CATTRIBUTES in varchar2 default null);
  procedure FORMRADIO(CNAME       in varchar2,
                      CVALUE      in varchar2,
                      CCHECKED    in varchar2 default null,
                      CATTRIBUTES in varchar2 default null);
  procedure FORMRESET(CVALUE      in varchar2 default 'Reset',
                      CATTRIBUTES in varchar2 default null);
  procedure FORMSUBMIT(CNAME       in varchar2 default null,
                       CVALUE      in varchar2 default 'Submit',
                       CATTRIBUTES in varchar2 default null);
  procedure FORMTEXT(CNAME       in varchar2,
                     CSIZE       in varchar2 default null,
                     CMAXLENGTH  in varchar2 default null,
                     CVALUE      in varchar2 default null,
                     CATTRIBUTES in varchar2 default null);
  procedure FORMSELECTOPEN(CNAME       in varchar2,
                           CPROMPT     in varchar2 default null,
                           NSIZE       in integer  default null,
                           CATTRIBUTES in varchar2 default null);
  procedure FORMSELECTOPTION(CVALUE      in varchar2,
                             CSELECTED   in varchar2 default null,
                             CATTRIBUTES in varchar2 default null);
  procedure FORMSELECTCLOSE;
  procedure FORMTEXTAREA(CNAME       in varchar2,
                         NROWS       in integer,
                         NCOLUMNS    in integer,
                         CALIGN      in varchar2 default null,
                         CATTRIBUTES in varchar2 default null); 
  procedure FORMTEXTAREA2(CNAME      in varchar2,
                          NROWS       in integer,
                          NCOLUMNS    in integer,
                          CALIGN      in varchar2 default null,
                          CWRAP       in varchar2 default null,
                          CATTRIBUTES in varchar2 default null); 
  procedure FORMTEXTAREAOPEN(CNAME       in varchar2,
                             NROWS       in integer,
                             NCOLUMNS    in integer,
                             CALIGN      in varchar2 default null,
                             CATTRIBUTES in varchar2 default null); 
  procedure FORMTEXTAREAOPEN2(CNAME       in varchar2,
                             NROWS       in integer,
                             NCOLUMNS    in integer,
                             CALIGN      in varchar2 default null,
                             CWRAP       in varchar2 default null,
                             CATTRIBUTES in varchar2 default null); 
  procedure FORMTEXTAREACLOSE;
  procedure FORMCLOSE;
--
  /* ### FUNCTIONS THAT APPEAR TO BE MISSING IN THE STANDARD PACKAGE ### */
  procedure PARACLOSE;
  procedure DIVCLOSE;
--
  procedure INIT;
--
end HTP;
/

create or replace package body HTP as
--
  AMP  constant varchar2(30) := '&'||'amp;';
  QUOT constant varchar2(30) := '&'||'quot;';
  LT   constant varchar2(30) := '&'||'lt;';
  GT   constant varchar2(30) := '&'||'gt;';
--
  procedure PRN(CBUF in varchar2 default null) is
  begin
    OWA.PRN(CBUF);
  end PRN;
  procedure PRN(DVAL in date) is
  begin
    OWA.PRN(to_char(DVAL));
  end PRN;
--
  procedure PRN(NVAL in number) is
  begin
    OWA.PRN(to_char(NVAL));
  end PRN;
--
  procedure PRINT(CBUF in varchar2 default null) is
  begin
    OWA.PRINT(CBUF);
  end PRINT;
--
  procedure PRINT(DVAL in date) is
  begin
    OWA.PRINT(to_char(DVAL));
  end PRINT;
--
  procedure PRINT(NVAL in number) is
  begin
    OWA.PRINT(to_char(NVAL));
  end PRINT;
--
  procedure P(CBUF in varchar2 default null) is
  begin
    OWA.PRINT(CBUF);
  end P;
--
  procedure P(DVAL in date) is
  begin
    OWA.PRINT(to_char(DVAL));
  end P;
--
  procedure P(NVAL in number) is
  begin
    OWA.PRINT(to_char(NVAL));
  end P;
--
  procedure PRINT_TAGGED_TEXT(TAG in varchar2, CTEXT in varchar2,
                              ATTRS in varchar2, N in number,
                              NAMES in OWA.VCTAB, VALS in OWA.VCTAB,
                              CLOSE_FLAG in boolean default true) is
    BUF    varchar2(32767);
  begin
    BUF := '<'||TAG;
    for I in 1..N loop
      if (VALS(I) is not null) then
        if (NAMES(I) is null) then
          BUF := BUF||' '||VALS(I);
        elsif (VALS(I) = chr(0)) then
          BUF := BUF||' '||NAMES(I)||'=""';
        else
          BUF := BUF||' '||NAMES(I)||'="'||VALS(I)||'"';
        end if;
      end if;
    end loop;
    if (ATTRS is not null) then
      BUF := BUF||' '||ATTRS;
    end if;
    if (CLOSE_FLAG) then
      OWA.PRINT(BUF||'>'||CTEXT||'</'||TAG||'>');
    else
      OWA.PRINT(BUF||'>');
    end if;
  end PRINT_TAGGED_TEXT;
--
  procedure PRINT_TAGGED_TEXT(TAG in varchar2,
                              CTEXT in varchar2, ATTRS in varchar2) is
  begin
    if (ATTRS is not null) then
      OWA.PRINT('<'||TAG||' '||ATTRS||'>'||CTEXT||'</'||TAG||'>');
    else
      OWA.PRINT('<'||TAG||'>'||CTEXT||'</'||TAG||'>');
    end if;
  end PRINT_TAGGED_TEXT;
--
  procedure PRINT_TAGGED_TEXT(TAG in varchar2,
                              CTEXT in varchar2, ATTRS in varchar2,
                              NAME in varchar2, VAL in varchar2) is
    NAMES OWA.VCTAB;
    VALS  OWA.VCTAB;
  begin
    if ((NAME is null) and (VAL is null)) then
      PRINT_TAGGED_TEXT(TAG, CTEXT, ATTRS);
    else
      NAMES(1) := NAME;
      VALS(1) := VAL;
      PRINT_TAGGED_TEXT(TAG, CTEXT, ATTRS, 1, NAMES, VALS);
    end if;
  end PRINT_TAGGED_TEXT;
--
  procedure PRINT_TAG(TAG in varchar2, ATTRS in varchar2, N in number,
                      NAMES in OWA.VCTAB, VALS in OWA.VCTAB) is
  begin
    PRINT_TAGGED_TEXT(TAG, null, ATTRS, N, NAMES, VALS, false);
  end PRINT_TAG;
--
  procedure HTMLOPEN is
  begin
    OWA.PRINT('<html>');
  end HTMLOPEN;
--
  procedure HEADOPEN is
  begin
    OWA.PRINT('<head>');
  end HEADOPEN;
--
  procedure BODYOPEN(CBACKGROUND in varchar2 default null,
                     CATTRIBUTES in varchar2 default null) is
    NAMES OWA.VCTAB;
    VALS  OWA.VCTAB;
  begin
    NAMES(1) := 'background';
    VALS(1) := CBACKGROUND;
    PRINT_TAG('body', CATTRIBUTES, 1, NAMES, VALS);
  end BODYOPEN;
--
  procedure FRAMESETOPEN(CROWS       in varchar2 default null,
                         CCOLS       in varchar2 default null,
                         CATTRIBUTES in varchar2 default null) is
    NAMES OWA.VCTAB;
    VALS  OWA.VCTAB;
  begin
    NAMES(1) := 'rows';
    VALS(1) := CROWS;
    NAMES(2) := 'cols';
    VALS(2) := CCOLS;
    PRINT_TAG('frameset', CATTRIBUTES, 2, NAMES, VALS);
  end FRAMESETOPEN;
--
  procedure MAPOPEN(CNAME       in varchar2,
                    CATTRIBUTES in varchar2 default null) is
    NAMES OWA.VCTAB;
    VALS  OWA.VCTAB;
  begin
    NAMES(1) := 'name';
    VALS(1) := CNAME;
    PRINT_TAG('map', CATTRIBUTES, 1, NAMES, VALS);
  end MAPOPEN;
--
  procedure NOFRAMESOPEN is
  begin
    OWA.PRINT('<noframes>');
  end NOFRAMESOPEN;
--
  procedure CENTEROPEN is
  begin
    OWA.PRINT('<center>');
  end CENTEROPEN;
--
  procedure LISTINGOPEN is
  begin
    OWA.PRINT('<listing>');
  end LISTINGOPEN;
--
  procedure PREOPEN(CCLEAR      in varchar2 default null,
                    CWIDTH      in varchar2 default null,
                    CATTRIBUTES in varchar2 default null) is
    NAMES OWA.VCTAB;
    VALS  OWA.VCTAB;
  begin
    NAMES(1) := 'clear';
    VALS(1) := CCLEAR;
    NAMES(2) := 'width';
    VALS(2) := CWIDTH;
    PRINT_TAG('pre', CATTRIBUTES, 2, NAMES, VALS);
  end PREOPEN;
--
  procedure BLOCKQUOTEOPEN(CNOWRAP     in varchar2 default null,
                           CCLEAR      in varchar2 default null,
                           CATTRIBUTES in varchar2 default null) is
    NAMES OWA.VCTAB;
    VALS  OWA.VCTAB;
  begin
    NAMES(1) := 'clear';
    VALS(1) := CCLEAR;
    if (CNOWRAP is null) then
      PRINT_TAG('blockquote', CATTRIBUTES, 1, NAMES, VALS);
    else
      NAMES(2) := '';
      VALS(2) := 'nowrap';
      PRINT_TAG('blockquote', CATTRIBUTES, 2, NAMES, VALS);
    end if;
  end BLOCKQUOTEOPEN;
--
  procedure DIRLISTOPEN is
  begin
    OWA.PRINT('<dir>');
  end DIRLISTOPEN;
--
  procedure MENULISTOPEN is
  begin
    OWA.PRINT('<menu>');
  end MENULISTOPEN;
--
  procedure ULISTOPEN(CCLEAR      in varchar2 default null,
                      CWRAP       in varchar2 default null,
                      CDINGBAT    in varchar2 default null,
                      CSRC        in varchar2 default null,
                      CATTRIBUTES in varchar2 default null) is
    NAMES OWA.VCTAB;
    VALS  OWA.VCTAB;
  begin
    NAMES(1) := 'clear';
    VALS(1) := CCLEAR;
    NAMES(2) := 'wrap';
    VALS(2) := CWRAP;
    NAMES(3) := 'dingbat';
    VALS(3) := CDINGBAT;
    NAMES(4) := 'src';
    VALS(4) := CSRC;
    PRINT_TAG('ul', CATTRIBUTES, 4, NAMES, VALS);
  end ULISTOPEN;
  procedure OLISTOPEN(CCLEAR      in varchar2 default null,
                      CWRAP       in varchar2 default null,
                      CATTRIBUTES in varchar2 default null) is
    NAMES OWA.VCTAB;
    VALS  OWA.VCTAB;
  begin
    NAMES(1) := 'clear';
    VALS(1) := CCLEAR;
    NAMES(2) := 'wrap';
    VALS(2) := CWRAP;
    PRINT_TAG('ol', CATTRIBUTES, 2, NAMES, VALS);
  end OLISTOPEN;
  procedure DLISTOPEN(CCLEAR      in varchar2 default null,
                      CATTRIBUTES in varchar2 default null) is
    NAMES OWA.VCTAB;
    VALS  OWA.VCTAB;
  begin
    NAMES(1) := 'clear';
    VALS(1) := CCLEAR;
    PRINT_TAG('dl', CATTRIBUTES, 1, NAMES, VALS);
  end DLISTOPEN;
--
  procedure HTMLCLOSE is
  begin
    OWA.PRINT('</html>');
  end HTMLCLOSE;
--
  procedure HEADCLOSE is
  begin
    OWA.PRINT('</head>');
  end HEADCLOSE;
--
  procedure BODYCLOSE is
  begin
    OWA.PRINT('</body>');
  end BODYCLOSE;
--
  procedure FRAMESETCLOSE is
  begin
    OWA.PRINT('</frameset>');
  end FRAMESETCLOSE;
--
  procedure MAPCLOSE is
  begin
    OWA.PRINT('</map>');
  end MAPCLOSE;
--
  procedure NOFRAMESCLOSE is
  begin
    OWA.PRINT('</noframes>');
  end NOFRAMESCLOSE;
--
  procedure CENTERCLOSE is
  begin
    OWA.PRINT('</center>');
  end CENTERCLOSE;
--
  procedure LISTINGCLOSE is
  begin
    OWA.PRINT('</listing>');
  end LISTINGCLOSE;
--
  procedure PRECLOSE is
  begin
    OWA.PRINT('</pre>');
  end PRECLOSE;
--
  procedure BLOCKQUOTECLOSE is
  begin
    OWA.PRINT('</blockquote>');
  end BLOCKQUOTECLOSE;
--
  procedure DIRLISTCLOSE is
  begin
    OWA.PRINT('</dir>');
  end DIRLISTCLOSE;
--
  procedure MENULISTCLOSE is
  begin
    OWA.PRINT('</menu>');
  end MENULISTCLOSE;
--
  procedure ULISTCLOSE is
  begin
    OWA.PRINT('</ul>');
  end ULISTCLOSE;
--
  procedure OLISTCLOSE is
  begin
    OWA.PRINT('</ol>');
  end OLISTCLOSE;
--
  procedure DLISTCLOSE is
  begin
    OWA.PRINT('</ul>');
  end DLISTCLOSE;
--
  procedure TITLE(CTITLE in varchar) is
  begin
    OWA.PRINT('<title>'||CTITLE||'</title>');
  end TITLE;
--
  procedure HR(CCLEAR      in varchar2 default null,
               CSRC        in varchar2 default null,
               CATTRIBUTES in varchar2 default null) is
    NAMES OWA.VCTAB;
    VALS  OWA.VCTAB;
  begin
    NAMES(1) := 'clear';
    VALS(1) := CCLEAR;
    NAMES(2) := 'src';
    VALS(2) := CSRC;
    PRINT_TAG('hr', CATTRIBUTES, 2, NAMES, VALS);
  end HR;
--
  procedure LINE(CCLEAR      in varchar2 default null,
                 CSRC        in varchar2 default null,
                 CATTRIBUTES in varchar2 default null) is
  begin
    HR(CCLEAR, CSRC, CATTRIBUTES);
  end LINE;
--
  procedure BR(CCLEAR      in varchar2 default null,
               CATTRIBUTES in varchar2 default null) is
    NAMES OWA.VCTAB;
    VALS  OWA.VCTAB;
  begin
    NAMES(1) := 'clear';
    VALS(1) := CCLEAR;
    PRINT_TAG('br', CATTRIBUTES, 1, NAMES, VALS);
  end BR;
--
  procedure NL(CCLEAR      in varchar2 default null,
               CATTRIBUTES in varchar2 default null) is
  begin
    BR(CCLEAR, CATTRIBUTES);
  end NL;
--
  procedure IMG2(CSRC        in varchar2,
                 CALIGN      in varchar2 default null,
                 CALT        in varchar2 default null,
                 CISMAP      in varchar2 default null,
                 CUSEMAP     in varchar2 default null,
                 CATTRIBUTES in varchar2 default null) is
    NAMES OWA.VCTAB;
    VALS  OWA.VCTAB;
  begin
    NAMES(1) := 'src';
    if (CSRC is null) then
      VALS(1) := chr(0); -- force the SRC= attribute to print
    else
      VALS(1) := CSRC;
    end if;
    NAMES(2) := 'align';
    VALS(2) := CALIGN;
    NAMES(3) := 'usemap';
    VALS(3) := CUSEMAP;
    if (CISMAP is null) then
      PRINT_TAG('img', CATTRIBUTES, 3, NAMES, VALS);
    else
      NAMES(4) := '';
      VALS(4) := 'ismap';
      PRINT_TAG('img', CATTRIBUTES, 4, NAMES, VALS);
    end if;
  end IMG2;
--
  procedure IMG(CSRC        in varchar2,
                CALIGN      in varchar2 default null,
                CALT        in varchar2 default null,
                CISMAP      in varchar2 default null,
                CATTRIBUTES in varchar2 default null) is
  begin
    IMG2(CSRC, CALIGN, CALT, CISMAP, null, CATTRIBUTES);
  end IMG;
--
  procedure AREA(CCOORDS     in varchar2,
                 CSHAPE      in varchar2 default null,
                 CHREF       in varchar2 default null,
                 CNOHREF     in varchar2 default null,
                 CTARGET     in varchar2 default null,
                 CATTRIBUTES in varchar2 default null) is
    NAMES OWA.VCTAB;
    VALS  OWA.VCTAB;
  begin
    NAMES(1) := 'coords';
    if (CCOORDS is null) then
      VALS(1) := chr(0); -- force the COORDS= attribute to print
    else
      VALS(1) := CCOORDS;
    end if;
    NAMES(2) := 'shape';
    VALS(2) := CSHAPE;
    NAMES(3) := 'href';
    VALS(3) := CHREF;
    NAMES(4) := 'target';
    VALS(4) := CTARGET;
    if (CNOHREF is null) then
      PRINT_TAG('area', CATTRIBUTES, 4, NAMES, VALS);
    else
      NAMES(5) := '';
      VALS(5) := 'nohref';
      PRINT_TAG('area', CATTRIBUTES, 5, NAMES, VALS);
    end if;
  end AREA;
--
  procedure FRAME(CSRC          in varchar2,
                  CNAME         in varchar2 default null,
                  CMARGINWIDTH  in varchar2 default null,
                  CMARGINHEIGHT in varchar2 default null,
                  CSCROLLING    in varchar2 default null,
                  CNORESIZE     in varchar2 default null,
                  CATTRIBUTES   in varchar2 default null) is
    NAMES OWA.VCTAB;
    VALS  OWA.VCTAB;
  begin
    NAMES(1) := 'src';
    VALS(1) := CSRC;
    NAMES(2) := 'name';
    VALS(2) := CNAME;
    NAMES(3) := 'marginwidth';
    VALS(3) := CMARGINWIDTH;
    NAMES(4) := 'marginheight';
    VALS(4) := CMARGINHEIGHT;
    NAMES(5) := 'scrolling';
    VALS(5) := CSCROLLING;
    if (CNORESIZE is null) then
      PRINT_TAG('frame', CATTRIBUTES, 5, NAMES, VALS);
    else
      NAMES(6) := '';
      VALS(6) := 'noresize';
      PRINT_TAG('frame', CATTRIBUTES, 6, NAMES, VALS);
    end if;
  end FRAME;
--
  procedure HEADER(NSIZE       in integer,
                   CTEXT       in varchar2,
                   CALIGN      in varchar2 default null,
                   CNOWRAP     in varchar2 default null,
                   CCLEAR      in varchar2 default null,
                   CATTRIBUTES in varchar2 default null) is
    NAMES OWA.VCTAB;
    VALS  OWA.VCTAB;
  begin
    NAMES(1) := 'align';
    VALS(1) := CALIGN;
    NAMES(2) := 'clear';
    VALS(2) := CCLEAR;
    if (CNOWRAP is null) then
      PRINT_TAGGED_TEXT('h'||to_char(NSIZE), CTEXT, CATTRIBUTES, 2,NAMES,VALS);
    else
      NAMES(3) := '';
      VALS(3) := 'nowrap';
      PRINT_TAGGED_TEXT('h'||to_char(NSIZE), CTEXT, CATTRIBUTES, 3,NAMES,VALS);
    end if;
  end HEADER;
--
  procedure HTITLE(CTITLE      in varchar2,
                   NSIZE       in integer default 1,
                   CALIGN      in varchar2 default null,
                   CNOWRAP     in varchar2 default null,
                   CCLEAR      in varchar2 default null,
                   CATTRIBUTES in varchar2 default null) is
  begin
    TITLE(CTITLE);
    HEADER(NSIZE, CTITLE, CALIGN, CNOWRAP, CCLEAR, CATTRIBUTES);
  end HTITLE;
--
  procedure ANCHOR2(CURL        in varchar2,
                    CTEXT       in varchar2, 
                    CNAME       in varchar2 default null,
                    CTARGET     in varchar2 default null,
                    CATTRIBUTES in varchar2 default null) is
    NAMES OWA.VCTAB;
    VALS  OWA.VCTAB;
  begin
    if (CURL is null) then
      NAMES(1) := 'name';
      if (CNAME is null) then
        VALS(1) := chr(0); -- force the NAME= attribute to print
      else
        VALS(1) := CNAME;
      end if;
      PRINT_TAGGED_TEXT('a', CTEXT, CATTRIBUTES, 1, NAMES, VALS);
    else
      NAMES(1) := 'href';
      VALS(1) := CURL;
      NAMES(2) := 'name';
      VALS(2) := CNAME;
      NAMES(3) := 'target';
      VALS(3) := CTARGET;
      PRINT_TAGGED_TEXT('a', CTEXT, CATTRIBUTES, 3, NAMES, VALS);
    end if;
  end ANCHOR2;
--
  procedure ANCHOR(CURL        in varchar2,
                   CTEXT       in varchar2, 
                   CNAME       in varchar2 default null,
                   CATTRIBUTES in varchar2 default null) is
  begin
    ANCHOR2(CURL, CTEXT, CNAME, null, CATTRIBUTES);
  end ANCHOR;
--
  procedure LISTHEADER(CTEXT       in varchar2,
                       CATTRIBUTES in varchar2 default null) is
  begin
    PRINT_TAGGED_TEXT('lh', CTEXT, CATTRIBUTES);
  end LISTHEADER;
--
  procedure LISTITEM(CTEXT       in varchar2 default null,
                     CCLEAR      in varchar2 default null,
                     CDINGBAT    in varchar2 default null,
                     CSRC        in varchar2 default null,
                     CATTRIBUTES in varchar2 default null) is
    NAMES OWA.VCTAB;
    VALS  OWA.VCTAB;
  begin
    NAMES(1) := 'clear';
    VALS(1) := CCLEAR;
    NAMES(2) := 'dingbat';
    VALS(2) := CDINGBAT;
    NAMES(3) := 'src';
    VALS(3) := CSRC;
    if (CTEXT is null) then
      PRINT_TAG('li', CATTRIBUTES, 3, NAMES, VALS);
    else
      PRINT_TAGGED_TEXT('li', CTEXT, CATTRIBUTES, 3, NAMES, VALS);
    end if;
  end LISTITEM;
--
  procedure DLISTTERM(CTEXT       in varchar2 default null,
                      CCLEAR      in varchar2 default null,
                      CATTRIBUTES in varchar2 default null) is
  begin
    PRINT_TAGGED_TEXT('dt', CTEXT, CATTRIBUTES, 'clear', CCLEAR);
  end DLISTTERM;
--
  procedure DLISTDEF(CTEXT       in varchar2 default null,
                     CCLEAR      in varchar2 default null,
                     CATTRIBUTES in varchar2 default null) is
  begin
    PRINT_TAGGED_TEXT('dd', CTEXT, CATTRIBUTES, 'clear', CCLEAR);
  end DLISTDEF;
--
  procedure PARA is
  begin
    OWA.PRINT('<p>');
  end PARA;
--
  procedure PARAGRAPH(CALIGN      in varchar2 default null,
                      CNOWRAP     in varchar2 default null,
                      CCLEAR      in varchar2 default null,
                      CATTRIBUTES in varchar2 default null) is
    NAMES OWA.VCTAB;
    VALS  OWA.VCTAB;
  begin
    NAMES(1) := 'align';
    VALS(1) := CALIGN;
    NAMES(2) := 'clear';
    VALS(2) := CCLEAR;
    if (CNOWRAP is null) then
      PRINT_TAG('p', CATTRIBUTES, 2, NAMES, VALS);
    else
      NAMES(3) := '';
      VALS(3) := 'nowrap';
      PRINT_TAG('p', CATTRIBUTES, 3, NAMES, VALS);
    end if;
  end PARAGRAPH;
--
  procedure DIV(CALIGN      in varchar2 default null,
                CATTRIBUTES in varchar2 default null) is
    NAMES OWA.VCTAB;
    VALS  OWA.VCTAB;
  begin
    NAMES(1) := 'align';
    VALS(1) := CALIGN;
    PRINT_TAG('div', CATTRIBUTES, 1, NAMES, VALS);
  end DIV;
--
  procedure WBR is
  begin
    OWA.PRINT('<wbr>');
  end WBR;
--
  procedure COMMENT(CTEXT in varchar2) is
  begin
    OWA.PRINT('<!-- '||CTEXT||' -->');
  end COMMENT;
--
  procedure NOBR(CTEXT in varchar2) is
  begin
    OWA.PRINT('<nobr>'||CTEXT||'</nobr>');
  end NOBR;
--
  procedure CENTER(CTEXT in varchar2) is
  begin
    OWA.PRINT('<center>'||CTEXT||'</center>');
  end CENTER;
--
  procedure DFN(CTEXT       in varchar2,
                CATTRIBUTES in varchar2 default null) is
  begin
    PRINT_TAGGED_TEXT('dfn', CTEXT, CATTRIBUTES);
  end DFN;
--
  procedure CITE(CTEXT       in varchar2,
                 CATTRIBUTES in varchar2 default null) is
  begin
    PRINT_TAGGED_TEXT('cite', CTEXT, CATTRIBUTES);
  end CITE;
--
  procedure CODE(CTEXT       in varchar2,
                 CATTRIBUTES in varchar2 default null) is
  begin
    PRINT_TAGGED_TEXT('code', CTEXT, CATTRIBUTES);
  end CODE;
--
  procedure EM(CTEXT       in varchar2,
               CATTRIBUTES in varchar2 default null) is
  begin
    PRINT_TAGGED_TEXT('em', CTEXT, CATTRIBUTES);
  end EM;
--
  procedure EMPHASIS(CTEXT       in varchar2,
                     CATTRIBUTES in varchar2 default null) is
  begin
    PRINT_TAGGED_TEXT('em', CTEXT, CATTRIBUTES);
  end EMPHASIS;
--
  procedure KBD(CTEXT       in varchar2,
                CATTRIBUTES in varchar2 default null) is
  begin
    PRINT_TAGGED_TEXT('kbd', CTEXT, CATTRIBUTES);
  end KBD;
--
  procedure KEYBOARD(CTEXT       in varchar2,
                     CATTRIBUTES in varchar2 default null) is
  begin
    PRINT_TAGGED_TEXT('kbd', CTEXT, CATTRIBUTES);
  end KEYBOARD;
--
  procedure SAMPLE(CTEXT       in varchar2,
                   CATTRIBUTES in varchar2 default null) is
  begin
    PRINT_TAGGED_TEXT('samp', CTEXT, CATTRIBUTES);
  end SAMPLE;
--
  procedure VARIABLE(CTEXT       in varchar2,
                     CATTRIBUTES in varchar2 default null) is
  begin
    PRINT_TAGGED_TEXT('var', CTEXT, CATTRIBUTES);
  end VARIABLE;
--
  procedure STRONG(CTEXT       in varchar2,
                   CATTRIBUTES in varchar2 default null) is
  begin
    PRINT_TAGGED_TEXT('strong', CTEXT, CATTRIBUTES);
  end STRONG;
--
  procedure BIG(CTEXT       in varchar2,
                CATTRIBUTES in varchar2 default null) is
  begin
    PRINT_TAGGED_TEXT('big', CTEXT, CATTRIBUTES);
  end BIG;
--
  procedure SMALL(CTEXT       in varchar2,
                  CATTRIBUTES in varchar2 default null) is
  begin
    PRINT_TAGGED_TEXT('small', CTEXT, CATTRIBUTES);
  end SMALL;
--
  procedure BOLD(CTEXT       in varchar2,
                 CATTRIBUTES in varchar2 default null) is
  begin
    PRINT_TAGGED_TEXT('b', CTEXT, CATTRIBUTES);
  end;
--
  procedure ITALIC(CTEXT       in varchar2,
                   CATTRIBUTES in varchar2 default null) is
  begin
    PRINT_TAGGED_TEXT('i', CTEXT, CATTRIBUTES);
  end;
--
  procedure UNDERLINE(CTEXT       in varchar2,
                      CATTRIBUTES in varchar2 default null) is
  begin
    PRINT_TAGGED_TEXT('u', CTEXT, CATTRIBUTES);
  end;
--
  procedure PLAINTEXT(CTEXT       in varchar2,
                      CATTRIBUTES in varchar2 default null) is
  begin
    PRINT_TAGGED_TEXT('plaintext', CTEXT, CATTRIBUTES);
  end PLAINTEXT;
--
  procedure TELETYPE(CTEXT       in varchar2,
                     CATTRIBUTES in varchar2 default null) is
  begin
    PRINT_TAGGED_TEXT('tt', CTEXT, CATTRIBUTES);
  end TELETYPE;
--
  procedure S(CTEXT       in varchar2,
              CATTRIBUTES in varchar2 default null) is
  begin
    PRINT_TAGGED_TEXT('s', CTEXT, CATTRIBUTES);
  end S;
--
  procedure STRIKE(CTEXT       in varchar2,
                   CATTRIBUTES in varchar2 default null) is
  begin
    PRINT_TAGGED_TEXT('strike', CTEXT, CATTRIBUTES);
  end STRIKE;
--
  procedure SUB(CTEXT       in varchar2,
                CALIGN      in varchar2 default null,
                CATTRIBUTES in varchar2 default null) is
  begin
    PRINT_TAGGED_TEXT('sub', CTEXT, CATTRIBUTES, 'align', CALIGN);
  end SUB;
--
  procedure SUP(CTEXT       in varchar2,
                CALIGN      in varchar2 default null,
                CATTRIBUTES in varchar2 default null) is
  begin
    PRINT_TAGGED_TEXT('sup', CTEXT, CATTRIBUTES, 'align', CALIGN);
  end SUP;
--
  procedure TABLEOPEN(CBORDER     in varchar2 default null,
                      CALIGN      in varchar2 default null,
                      CNOWRAP     in varchar2 default null,
                      CCLEAR      in varchar2 default null,
                      CATTRIBUTES in varchar2 default null) is
    NAMES OWA.VCTAB;
    VALS  OWA.VCTAB;
  begin
    NAMES(1) := '';
    VALS(1) := CBORDER;
    NAMES(2) := 'align';
    VALS(2) := CALIGN;
    NAMES(3) := 'clear';
    VALS(3) := CCLEAR;
    if (CNOWRAP is null) then
      PRINT_TAG('table', CATTRIBUTES, 3, NAMES, VALS);
    else
      NAMES(4) := '';
      VALS(4) := 'nowrap';
      PRINT_TAG('table', CATTRIBUTES, 4, NAMES, VALS);
    end if;
  end TABLEOPEN;
--
  procedure TABLECAPTION(CCAPTION    in varchar2,
                         CALIGN      in varchar2 default null,
                         CATTRIBUTES in varchar2 default null) is
  begin
    PRINT_TAGGED_TEXT('caption', CCAPTION, CATTRIBUTES, 'align', CALIGN);
  end TABLECAPTION;
--
  procedure TABLEROWOPEN(CALIGN      in varchar2 default null,
                         CVALIGN     in varchar2 default null,
                         CDP         in varchar2 default null,
                         CNOWRAP     in varchar2 default null,
                         CATTRIBUTES in varchar2 default null) is
    NAMES OWA.VCTAB;
    VALS  OWA.VCTAB;
  begin
    NAMES(1) := 'align';
    VALS(1) := CALIGN;
    NAMES(2) := 'valign';
    VALS(2) := CVALIGN;
    NAMES(3) := 'dp';
    VALS(3) := CDP;
    if (CNOWRAP is null) then
      PRINT_TAG('tr', CATTRIBUTES, 3, NAMES, VALS);
    else
      NAMES(4) := '';
      VALS(4) := 'nowrap';
      PRINT_TAG('tr', CATTRIBUTES, 4, NAMES, VALS);
    end if;
  end TABLEROWOPEN;
--
  procedure TABLEHEADER(CVALUE      in varchar2 default null,
                        CALIGN      in varchar2 default null,
                        CDP         in varchar2 default null,
                        CNOWRAP     in varchar2 default null,
                        CROWSPAN    in varchar2 default null,
                        CCOLSPAN    in varchar2 default null,
                        CATTRIBUTES in varchar2 default null) is
    NAMES OWA.VCTAB;
    VALS  OWA.VCTAB;
  begin
    NAMES(1) := 'align';
    VALS(1) := CALIGN;
    NAMES(2) := 'dp';
    VALS(2) := CDP;
    NAMES(3) := 'rowspan';
    VALS(3) := CROWSPAN;
    NAMES(4) := 'colspan';
    VALS(4) := CCOLSPAN;
    if (CNOWRAP is null) then
      PRINT_TAGGED_TEXT('th', CVALUE, CATTRIBUTES, 4, NAMES, VALS);
    else
      NAMES(5) := '';
      VALS(5) := 'nowrap';
      PRINT_TAGGED_TEXT('th', CVALUE, CATTRIBUTES, 5, NAMES, VALS);
    end if;
  end TABLEHEADER;
--
  procedure TABLEDATA(CVALUE      in varchar2 default null,
                      CALIGN      in varchar2 default null,
                      CDP         in varchar2 default null,
                      CNOWRAP     in varchar2 default null,
                      CROWSPAN    in varchar2 default null,
                      CCOLSPAN    in varchar2 default null,
                      CATTRIBUTES in varchar2 default null) is
    NAMES OWA.VCTAB;
    VALS  OWA.VCTAB;
  begin
    NAMES(1) := 'align';
    VALS(1) := CALIGN;
    NAMES(2) := 'dp';
    VALS(2) := CDP;
    NAMES(3) := 'rowspan';
    VALS(3) := CROWSPAN;
    NAMES(4) := 'colspan';
    VALS(4) := CCOLSPAN;
    if (CNOWRAP is null) then
      PRINT_TAGGED_TEXT('td', CVALUE, CATTRIBUTES, 4, NAMES, VALS);
    else
      NAMES(5) := '';
      VALS(5) := 'nowrap';
      PRINT_TAGGED_TEXT('td', CVALUE, CATTRIBUTES, 5, NAMES, VALS);
    end if;
  end TABLEDATA;
--
  procedure TABLEROWCLOSE is
  begin
    OWA.PRINT('</tr>');
  end TABLEROWCLOSE;
--
  procedure TABLECLOSE is
  begin
    OWA.PRINT('</table>');
  end TABLECLOSE;
--
  procedure PARACLOSE is
  begin
    OWA.PRINT('</p>');
  end PARACLOSE;
--
  procedure DIVCLOSE is
  begin
    OWA.PRINT('</div>');
  end DIVCLOSE;
--
  procedure FORMOPEN(CURL        in varchar2,
                     CMETHOD     in varchar2 default 'POST',
                     CTARGET     in varchar2 default null,
                     CENCTYPE    in varchar2 default null, 
                     CATTRIBUTES in varchar2 default null) is
    NAMES OWA.VCTAB;
    VALS  OWA.VCTAB;
  begin
    NAMES(1) := 'action';
    if (CURL is null) then
      VALS(1) := chr(0); -- force the ACTION= attribute to print
    else
      VALS(1) := CURL;
    end if;
    NAMES(2) := 'method';
    VALS(2) := CMETHOD;
    NAMES(3) := 'target';
    VALS(3) := CTARGET;
    NAMES(4) := 'enctype';
    VALS(4) := CENCTYPE;
    PRINT_TAG('form', CATTRIBUTES, 4, NAMES, VALS);
  end FORMOPEN;
--
  procedure FORMCHECKBOX(CNAME       in varchar2,
                         CVALUE      in varchar2 default 'on',
                         CCHECKED    in varchar2 default null,
                         CATTRIBUTES in varchar2 default null) is
    NAMES OWA.VCTAB;
    VALS  OWA.VCTAB;
    TAG   varchar2(100);
  begin
    NAMES(1) := 'name';
    if (CNAME is null) then
      VALS(1) := chr(0); -- force the NAME= attribute to print
    else
      VALS(1) := CNAME;
    end if;
    NAMES(2) := 'value';
    VALS(2) := CVALUE;
    if (CCHECKED is null) then
      TAG := 'input type="checkbox"';
    else
      TAG := 'input type="checkbox" checked';
    end if;
    PRINT_TAG(TAG, CATTRIBUTES, 2, NAMES, VALS);
  end FORMCHECKBOX;
--
  procedure FORMFILE(CNAME       in varchar2,
                     CACCEPT     in varchar2 default null,
                     CATTRIBUTES in varchar2 default null) is
    NAMES OWA.VCTAB;
    VALS  OWA.VCTAB;
  begin
    NAMES(1) := 'name';
    VALS(1) := CNAME;
    NAMES(2) := 'accept';
    VALS(2) := CACCEPT;
    PRINT_TAG('input type="file"', CATTRIBUTES, 2, NAMES, VALS);
  end FORMFILE;
--
  procedure FORMHIDDEN(CNAME       in varchar2,
                       CVALUE      in varchar2 default null,
                       CATTRIBUTES in varchar2 default null) is
    NAMES OWA.VCTAB;
    VALS  OWA.VCTAB;
  begin
    NAMES(1) := 'name';
    if (CNAME is null) then
      VALS(1) := chr(0); -- force the NAME= attribute to print
    else
      VALS(1) := CNAME;
    end if;
    NAMES(2) := 'value';
    if (CVALUE is null) then
      VALS(2) := chr(0); -- force the VALUE= attribute to print
    else
      VALS(2) := CVALUE;
    end if;
    PRINT_TAG('input type="hidden"', CATTRIBUTES, 2, NAMES, VALS);
  end FORMHIDDEN;
--
  procedure FORMIMAGE(CNAME       in varchar2,
                      CSRC        in varchar2,
                      CALIGN      in varchar2 default null,
                      CATTRIBUTES in varchar2 default null) is
    NAMES OWA.VCTAB;
    VALS  OWA.VCTAB;
  begin
    NAMES(1) := 'name';
    if (CNAME is null) then
      VALS(1) := chr(0); -- force the NAME= attribute to print
    else
      VALS(1) := CNAME;
    end if;
    NAMES(2) := 'src';
    if (CSRC is null) then
      VALS(2) := chr(0); -- force the SRC= attribute to print
    else
      VALS(2) := CSRC;
    end if;
    NAMES(3) := 'align';
    VALS(3) := CALIGN;
    PRINT_TAG('input type="image"', CATTRIBUTES, 3, NAMES, VALS);
  end FORMIMAGE;
--
  procedure FORMPASSWORD(CNAME       in varchar2,
                         CSIZE       in varchar2 default null,
                         CMAXLENGTH  in varchar2 default null,
                         CVALUE      in varchar2 default null,
                         CATTRIBUTES in varchar2 default null) is
    NAMES OWA.VCTAB;
    VALS  OWA.VCTAB;
  begin
    NAMES(1) := 'name';
    if (CNAME is null) then
      VALS(1) := chr(0); -- force the NAME= attribute to print
    else
      VALS(1) := CNAME;
    end if;
    NAMES(2) := 'size';
    VALS(2) := CSIZE;
    NAMES(3) := 'maxlength';
    VALS(3) := CMAXLENGTH;
    NAMES(4) := 'value';
    VALS(4) := CVALUE;
    PRINT_TAG('input type="password"', CATTRIBUTES, 4, NAMES, VALS);
  end FORMPASSWORD;
--
  procedure FORMRADIO(CNAME       in varchar2,
                      CVALUE      in varchar2,
                      CCHECKED    in varchar2 default null,
                      CATTRIBUTES in varchar2 default null) is
    NAMES OWA.VCTAB;
    VALS  OWA.VCTAB;
    TAG   varchar2(100);
  begin
    NAMES(1) := 'name';
    if (CNAME is null) then
      VALS(1) := chr(0); -- force the NAME= attribute to print
    else
      VALS(1) := CNAME;
    end if;
    NAMES(2) := 'value';
    if (CVALUE is null) then
      VALS(2) := chr(0); -- force the VALUE= attribute to print
    else
      VALS(2) := CVALUE;
    end if;
    if (CCHECKED is null) then
      TAG := 'input type="radio"';
    else
      TAG := 'input type="radio" checked';
    end if;
    PRINT_TAG(TAG, CATTRIBUTES, 2, NAMES, VALS);
  end FORMRADIO;
--
  procedure FORMRESET(CVALUE      in varchar2 default 'Reset',
                      CATTRIBUTES in varchar2 default null) is
    NAMES OWA.VCTAB;
    VALS  OWA.VCTAB;
  begin
    NAMES(1) := 'value';
    VALS(1) := CVALUE;
    PRINT_TAG('input type="reset"', CATTRIBUTES, 1, NAMES, VALS);
  end FORMRESET;
--
  procedure FORMSUBMIT(CNAME       in varchar2 default null,
                       CVALUE      in varchar2 default 'Submit',
                       CATTRIBUTES in varchar2 default null) is
    NAMES OWA.VCTAB;
    VALS  OWA.VCTAB;
  begin
    NAMES(1) := 'name';
    if (CNAME is null) then
      VALS(1) := chr(0); -- force the NAME= attribute to print
    else
      VALS(1) := CNAME;
    end if;
    NAMES(2) := 'value';
    if (CVALUE is null) then
      VALS(2) := chr(0); -- force the VALUE= attribute to print
    else
      VALS(2) := CVALUE;
    end if;
    PRINT_TAG('input type="submit"', CATTRIBUTES, 2, NAMES, VALS);
  end FORMSUBMIT;
--
  procedure FORMTEXT(CNAME       in varchar2,
                     CSIZE       in varchar2 default null,
                     CMAXLENGTH  in varchar2 default null,
                     CVALUE      in varchar2 default null,
                     CATTRIBUTES in varchar2 default null) is
    NAMES OWA.VCTAB;
    VALS  OWA.VCTAB;
  begin
    NAMES(1) := 'name';
    if (CNAME is null) then
      VALS(1) := chr(0); -- force the NAME= attribute to print
    else
      VALS(1) := CNAME;
    end if;
    NAMES(2) := 'size';
    VALS(2) := CSIZE;
    NAMES(3) := 'maxlength';
    VALS(3) := CMAXLENGTH;
    NAMES(4) := 'value';
    VALS(4) := CVALUE;
    PRINT_TAG('input type="text"', CATTRIBUTES, 4, NAMES, VALS);
  end FORMTEXT;
--
  procedure FORMSELECTOPEN(CNAME       in varchar2,
                           CPROMPT     in varchar2 default null,
                           NSIZE       in integer  default null,
                           CATTRIBUTES in varchar2 default null) is
    NAMES OWA.VCTAB;
    VALS  OWA.VCTAB;
  begin
    NAMES(1) := 'name';
    if (CNAME is null) then
      VALS(1) := chr(0); -- force the NAME= attribute to print
    else
      VALS(1) := CNAME;
    end if;
    NAMES(2) := 'prompt';
    VALS(2) := CPROMPT;
    NAMES(3) := 'size';
    VALS(3) := to_char(NSIZE);
    PRINT_TAG('select', CATTRIBUTES, 3, NAMES, VALS);
  end FORMSELECTOPEN;
--
  procedure FORMSELECTOPTION(CVALUE      in varchar2,
                             CSELECTED   in varchar2 default null,
                             CATTRIBUTES in varchar2 default null) is
    NAMES OWA.VCTAB;
    VALS  OWA.VCTAB;
    N     number;
  begin
    NAMES(1) := '';
    VALS(1) := 'selected';
    if (CSELECTED is null) then
      N := 0;
    else
      N := 1;
    end if;
    PRINT_TAGGED_TEXT('option', CVALUE, CATTRIBUTES, N, NAMES, VALS);
  end FORMSELECTOPTION;
--
  procedure FORMSELECTCLOSE is
  begin
    OWA.PRINT('</select>');
  end;
--
  procedure TEXT_AREA(CNAME       in varchar2,
                      NROWS       in integer,
                      NCOLUMNS    in integer,
                      CALIGN      in varchar2 default null,
                      CWRAP       in varchar2 default null,
                      CVALUE      in varchar2 default null,
                      CATTRIBUTES in varchar2 default null) is 
    NAMES OWA.VCTAB;
    VALS  OWA.VCTAB;
    TAG   varchar2(100);
  begin
    NAMES(1) := 'name';
    if (CNAME is null) then
      VALS(1) := chr(0); -- force the NAME= attribute to print
    else
      VALS(1) := CNAME;
    end if;
    NAMES(2) := 'rows';
    if (NROWS is null) then
      VALS(2) := chr(0); -- force the ROWS= attribute to print
    else
      VALS(2) := to_char(NROWS);
    end if;
    NAMES(3) := 'cols';
    if (NCOLUMNS is null) then
      VALS(2) := chr(0); -- force the COLS= attribute to print
    else
      VALS(2) := to_char(NCOLUMNS);
    end if;
    NAMES(4) := 'align';
    VALS(4) := CALIGN;
    if (CWRAP is null) then
      TAG := 'input type="textarea"';
    else
      TAG := 'input type="textarea" wrap';
    end if;
    if (CVALUE = chr(0)) then
      PRINT_TAG(TAG, CATTRIBUTES, 4, NAMES, VALS);
    else
      PRINT_TAGGED_TEXT(TAG, CVALUE, CATTRIBUTES, 4, NAMES, VALS);
    end if;
  end TEXT_AREA;
--
  procedure FORMTEXTAREA(CNAME       in varchar2,
                         NROWS       in integer,
                         NCOLUMNS    in integer,
                         CALIGN      in varchar2 default null,
                         CATTRIBUTES in varchar2 default null) is 
  begin
    TEXT_AREA(CNAME, NROWS, NCOLUMNS, CALIGN, null, '', CATTRIBUTES);
  end FORMTEXTAREA;
--
  procedure FORMTEXTAREA2(CNAME      in varchar2,
                          NROWS       in integer,
                          NCOLUMNS    in integer,
                          CALIGN      in varchar2 default null,
                          CWRAP       in varchar2 default null,
                          CATTRIBUTES in varchar2 default null) is 
  begin
    TEXT_AREA(CNAME, NROWS, NCOLUMNS, CALIGN, CWRAP, '', CATTRIBUTES);
  end FORMTEXTAREA2;
--
  procedure FORMTEXTAREAOPEN(CNAME       in varchar2,
                             NROWS       in integer,
                             NCOLUMNS    in integer,
                             CALIGN      in varchar2 default null,
                             CATTRIBUTES in varchar2 default null) is 
  begin
    TEXT_AREA(CNAME, NROWS, NCOLUMNS, CALIGN, null, chr(0), CATTRIBUTES);
  end FORMTEXTAREAOPEN;
--
  procedure FORMTEXTAREAOPEN2(CNAME       in varchar2,
                             NROWS       in integer,
                             NCOLUMNS    in integer,
                             CALIGN      in varchar2 default null,
                             CWRAP       in varchar2 default null,
                             CATTRIBUTES in varchar2 default null) is 
  begin
    TEXT_AREA(CNAME, NROWS, NCOLUMNS, CALIGN, CWRAP, chr(0), CATTRIBUTES);
  end FORMTEXTAREAOPEN2;
--
  procedure FORMTEXTAREACLOSE is
  begin
    OWA.PRINT('</textarea>');
  end FORMTEXTAREACLOSE;
--
  procedure FORMCLOSE is
  begin
    OWA.PRINT('</form>');
  end FORMCLOSE;
--
  function ESCAPE_ARG(ARG in varchar2) return varchar2 is
  begin
    return(replace(
             replace(
               replace(
                 replace(
                   replace(
                     replace(
                       replace(
                         replace(
                           replace(
                             replace(ARG,
                                     '%','%25'),
                                   '&','%26'),
                                 '=','%3D'),
                               '?','%3F'),
                             '+','%2B'),
                           '"','%22'),
                         '#','%23'),
                       '<','%3C'),
                     '>','%3E'),
                   ' ','+'));
  end ESCAPE_ARG;
--
  function ESCAPE_SC(CTEXT in varchar2) return varchar2 is
  begin
    return(replace(replace(replace(replace(CTEXT,
                                           '&', AMP),
                                   '"', QUOT),
                           '<', LT),
                   '>', GT));
  end ESCAPE_SC;
--
  /*
  ** ### WITHIN ARGUMENTS, YOU SHOULD DO SOME FURTHER REPLACEMENTS.
  ** ### THE PROBLEM IS, YOU CAN'T DO THAT AFTER THE URL HAS BEEN
  ** ### ASSEMBLED.  EVEN IF YOU PARSE THROUGH THE ARGUMENTS AND
  ** ### TRY TO FIX THEM, YOU'LL GET CONFUSED IF AN ARGUMENT VALUE
  ** ### HAPPENS TO CONTAIN A STOP CHARACTER SUCH AS AMPERSAND.
  ** ### SO, THIS ROUTINE SHOULD ONLY BE USED FOR THE PATH PORTION
  ** ### OF THE URL, AND ANOTHER ROUTINE SHOULD BE USED TO ESCAPE
  ** ### ANY ARGUMENTS DURING THE ASSEMBLY PROCESS.
  */
  function ESCAPE_URL(URL in varchar2) return varchar2 is
  begin
    return(replace(
             replace(
               replace(
                 replace(
                   replace(
                     replace(URL,
                             '%','%25'),
                           ' ','%20'),
                         '"','%22'),
                       '+','%2B'),
                     '<','%3C'),
                   '>','%3E'));
  end ESCAPE_URL;
--
  procedure INIT is
  begin
    OWA.INIT;
  end INIT;
end HTP;
/
